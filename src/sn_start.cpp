#include "sn_impl.h"
#include "sn_server.h"
#include "sn_log.h"
#include "sn_monitor.h"
#include "sn_timer.h"
#include "sn_socket.h"
#include <sstream>

struct monitor
{
	int count;
	SNMonitor **m;
	std::condition_variable cond;
	std::mutex m_mutex;
	int sleep;
};

struct worker_parm
{
	monitor *m;
	int id;
	int weight;
};

static void wakeup(struct monitor *m, int busy)
{
	if (m->sleep >= m->count - busy) {
		// signal sleep worker, "spurious wakeup" is harmless
		m->cond.notify_one();
	}
}

typedef std::shared_ptr<std::thread> ThreadPtr;
static std::list<ThreadPtr> g_threads;
static ThreadPtr g_socketTherad;
static ThreadPtr g_timerTherad;
static ThreadPtr g_monitorTherad;
//#define CHECK_ABORT if (SNServer::Get()->Total() == 0) break;
#define CHECK_ABORT if (SNServer::Get()->IsExit()) break;

// 定时器线程
static void _timer(void *p)
{
	monitor *m = (monitor *)p;
	SNServer::Get()->ThreadInit(THREAD_TIMER);
	for (;;) {
		skynet_updatetime();
		CHECK_ABORT;
		wakeup(m, m->count - 1);
		std::this_thread::sleep_for((std::chrono::milliseconds(10)));
	}

	// TODO :: 这里要退出SOCKET线程
	// wakeup socket thread
	skynet_socket_exit();

	// wakeup all worker thread
	m->cond.notify_all();
}


// IO线程
static void _socket(void *p)
{
	monitor *m = (monitor *)p;
	SNServer::Get()->ThreadInit(THREAD_SOCKET);
	for (;;) {
		CHECK_ABORT;
		int r = skynet_socket_poll();
		if (r == 0)
			break;
		if (r < 0) {
			continue;
		}
		wakeup(m, 0);
	}
}

// 监控线程
static void _monitor(monitor *m)
{
	int i;
	int n = m->count;
	SNServer::Get()->ThreadInit(THREAD_MONITOR);
	for (;;) {
		CHECK_ABORT;
		for (i = 0; i < n; i++) {
			m->m[i]->Check();
		}
		for (i = 0; i < 5; i++) {
			CHECK_ABORT;
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		}
	}
}

// 工作线程
static void _worker(worker_parm *wp)
{
	int id = wp->id;
	int weight = wp->weight;
	monitor *m = wp->m;
	SNMonitor *sm = m->m[id];
	SNServer::Get()->ThreadInit(THREAD_WORKER);
	SNMessageQueue *q = NULL;
	for (;;) {
		q = SNServer::Get()->DispatchMessageQueue(sm, q, weight);
		if (q == NULL) {
			std::unique_lock<std::mutex> lock(m->m_mutex);
			++m->sleep;
			// "spurious wakeup" is harmless,
			// because skynet_context_message_dispatch() can be call at any time.
			m->cond.wait(lock);

			// 在这里检查退出信号，保证所有的消息全部分发完毕才退出
			CHECK_ABORT;

			--m->sleep;
		}
	}
}

static bool bootstrap(SNServer &snserver, SNContextPtr &pLogger, const char *cmdline)
{
	stringstream ss(cmdline);
	string name, args;
	ss >> name >> args;
	SNContextPtr pCtx = snserver.NewContext(name.c_str(), args.c_str());
	if (!pCtx) {
		skynet_error(NULL, "Bootstrap error : %s\n", cmdline);
		snserver.DispatchAll(pLogger);
		return false;
	}
	return true;
}

static void _start(int thread)
{
	monitor m;
	m.count = thread;
	m.sleep = 0;
	m.m = new SNMonitor*[thread];
	for (int i = 0; i < thread; i++) {
		m.m[i] = new SNMonitor();
	}

	g_monitorTherad = ThreadPtr(new std::thread(std::bind(_monitor, &m)));
	g_timerTherad = ThreadPtr(new std::thread(std::bind(_timer, &m)));
	g_socketTherad = ThreadPtr(new std::thread(std::bind(_socket, &m)));

	static int weight[] = {
		-1, -1, -1, -1, 0, 0, 0, 0,
		1, 1, 1, 1, 1, 1, 1, 1,
		2, 2, 2, 2, 2, 2, 2, 2,
		3, 3, 3, 3, 3, 3, 3, 3, };

	worker_parm *wp = new worker_parm[thread];
	for (int i = 0; i < thread; i++) {
		wp[i].m = &m;
		wp[i].id = i;
		if (i < sizeof(weight) / sizeof(weight[0])) {
			wp[i].weight = weight[i];
		}
		else {
			wp[i].weight = 0;
		}
		ThreadPtr pWork(new std::thread(std::bind(_worker, &wp[i])));
		g_threads.push_back(pWork);
	}
	
	//////////////////////////////////////////////////////////////////////////
	// TODO :: 处理退出步骤,这里退出顺序很重要，具体效果还有待测试

	// 1: 等待socket线程最先退出来
	g_socketTherad->join();
	LogInfo("Socket Thread Exit\n");

	// 2: 等待定时器和监控线程退出来
	g_timerTherad->join();
	LogInfo("Timer Thread Exit\n");

	g_monitorTherad->join();
	LogInfo("Monitor Thread Exit\n");

	// 3: 等待工作线程将所有的消息分发完毕再退出来
	while (m.sleep != (int)g_threads.size()) {
		LogInfo("Wait DispatchMessageQueue\n");
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}
	m.cond.notify_all();
	for (auto it = g_threads.begin(); it != g_threads.end(); ++it) {
		(*it)->join();
	}
	LogInfo("Worket Thread Group Exit\n");

	// 3 : 释放所有数据
	SNServer::Get()->Release();

	delete[] wp;
	for (int i = 0; i < m.count; ++i) {
		delete m.m[i];
	}
	delete[] m.m;
}

static void ExitSNServer()
{
	SNServer::Get()->Exit();
}


void OnSignal(int s);
void skynet_start(SNServer &snserver, skynet_config *config)
{
	signal(SIGINT, OnSignal);
	signal(SIGTERM, OnSignal);
#ifdef _WINDOWS_
	signal(SIGBREAK, OnSignal);
#endif

	SNContextPtr pLogger = snserver.NewContext("logger", config->logger);
	if (pLogger == NULL) {
		LogError("Can't launch logger service\n");
		return;
	}
	if (!bootstrap(snserver, pLogger, config->bootstrap)) {
		return;
	}
	pLogger.reset(); //这里不要再引用了，生命周期留给全局句柄表(SNHandleMgr)管理

	_start(config->thread);
}

static void OnSignal(int s)
{
	switch (s)
	{
	case SIGINT:
	case SIGTERM:
#ifdef _WINDOWS_
	case SIGBREAK:
#endif
	{
		ExitSNServer();
		break;
	}
	}
	signal(s, OnSignal);
}
