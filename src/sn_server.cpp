#include "sn_server.h"
#include "sn_module.h"
#include "sn_impl.h"
#include "sn_malloc.h"
#include "sn_log.h"
#include "sn_env.h"
#include "sn_timer.h"
#include "sn_socket.h"

struct drop_t {
	uint32_t handle;
};

static void drop_message(sn_message *msg, void *ud)
{
	drop_t *d = (drop_t *)ud;
	sn_free(msg->data);
	uint32_t source = d->handle;
	assert(source);
	// report error to the message source
	skynet_send(NULL, source, msg->source, PTYPE_ERROR, 0, NULL, 0);
}

static void id_to_hex(char *str, uint32_t id)
{
	int i;
	static char hex[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
	str[0] = ':';
	for (i = 0; i < 8; i++) {
		str[i + 1] = hex[(id >> ((7 - i) * 4)) & 0xf];
	}
	str[9] = '\0';
}

//////////////////////////////////////////////////////////////////////////
// SNContext
SNContext::SNContext(SNModule *mod, void *inst)
{
	m_instance = inst;
	m_mod = mod;
	m_cb_ud = NULL;
	m_cb = NULL;
	m_queue = NULL;
	m_logfile = NULL;
	handle = 0;
	m_session_id = 0;
	m_init = false;
	endless = false;
}

SNContext::~SNContext()
{
	if (m_logfile.load()) {
		fclose(m_logfile);
	}

	if (m_instance && m_mod) {
		m_mod->Release(m_instance);
	}
}

bool SNContext::Init(SNMessageQueue *queue, uint32_t hanle, const char *parm)
{
	handle = hanle;
	m_queue = queue;

	CHECKCALLING_BEGIN(this);
	int r = m_mod->Init(m_instance, this, parm);
	CHECKCALLING_END(this);
	if (r == 0) {
		m_init = true;
		return true;
	}
	else {
		return false;
	}
}

void SNContext::DispatchMessage(sn_message *msg)
{
	if (m_cb == NULL) {
		sn_free(msg->data);
		return;
	}

	assert(m_init);
	CHECKCALLING_BEGIN(this);
	int type = msg->sz >> HANDLE_REMOTE_SHIFT;
	size_t sz = msg->sz & HANDLE_MASK;
	FILE *f = m_logfile;
	if (f) {
		sn_log_output(f, msg->source, type, msg->session, msg->data, sz);
	}
	if (!m_cb(this, m_cb_ud, type, msg->session, msg->source, msg->data, sz)) {
		sn_free(msg->data);
	}
	CHECKCALLING_END(this);
}

void SNContext::DispatchAll()
{
	sn_message msg;
	while (!m_queue->Pop(&msg, false)) {
		DispatchMessage(&msg);
	}
}

void SNContext::Push(sn_message *msg)
{
	m_queue->Push(msg);
}

void SNContext::Push(void * msg, size_t sz, uint32_t source, int type, int session)
{
	sn_message smsg;
	smsg.source = source;
	smsg.session = session;
	smsg.data = msg;
	smsg.sz = sz | type << HANDLE_REMOTE_SHIFT;
	Push(&smsg);
}

uint32_t SNContext::NewSession()
{
	int session = ++m_session_id;
	if (session <= 0) {
		m_session_id = 1;
		return 1;
	}
	return session;
}

//////////////////////////////////////////////////////////////////////////
// SNServer
static SNServer *g_SNServer = NULL;

SNServer::SNServer(int harbor, const char *module_path)
{
	// 测试一下vld否能检测到这个new内存泄漏
	// new char;

	m_bExit = false;
	m_pModuleMgr = new SNModuleMgr;
	m_pHandleMgr = new SNHandleMgr(harbor);
	m_pMqMgr = new SNMessageQueueMgr;

	assert(g_SNServer == NULL);
	g_SNServer = this;
	m_total = 0;
	m_monitor_exit = 0;
	m_init = 1;
	ThreadInit(THREAD_MAIN);
	m_pModuleMgr->SetPaths(module_path);
	skynet_harbor_init(harbor);
	skynet_timer_init();
	skynet_socket_init();
}

SNServer::~SNServer()
{
}

SNServer *SNServer::Get()
{
	return g_SNServer;
}

void SNServer::ThreadInit(uint32_t threadType)
{
	if (!m_handle_key.get()) {
		m_handle_key.reset(new uint32_t(threadType));
	}
	else {
		*m_handle_key.get() = threadType;
	}
}

SNContextPtr SNServer::NewContext(const char *name, const char *param)
{
	SNModule *pModule = m_pModuleMgr->Load(name);
	if (pModule == NULL) {
		return NULL;
	}

	void *inst = pModule->Create();
	if (inst == NULL) {
		return NULL;
	}

	SNContextPtr pContext(new SNContext(pModule, inst), std::bind(&SNServer::DeleteContext, this, std::placeholders::_1));
	uint32_t handle = m_pHandleMgr->Register(pContext);
	SNMessageQueue *pQueue = new SNMessageQueue(handle);
	++m_total;

	if (pContext->Init(pQueue, handle, param)) {
		m_pMqMgr->Push(pQueue);
		skynet_error(pContext.get(), "LAUNCH %s %s", name, param ? param : "");
		return pContext;
	}
	else {
		skynet_error(pContext.get(), "FAILED launch %s", name);
		m_pHandleMgr->Remove(handle);
		struct drop_t d = { handle };
		pQueue->Release(drop_message, &d);
		delete pQueue;
		return SNContextPtr();
	}
}

void SNServer::DeleteContext(SNContext *pContext)
{
	--m_total;
	delete pContext;
}

void SNServer::DispatchAll(SNContextPtr &pContext)
{
	// for skynet_error
	ThreadInit(pContext->Handle());
	pContext->DispatchAll();
}

void SNServer::Endless(uint32_t handle)
{
	SNContextPtr pContext = m_pHandleMgr->FindContext(handle);
	if (pContext) {
		pContext->Endless();
	}
}

SNMessageQueue *SNServer::DispatchMessageQueue(SNMonitor *pMonitor, SNMessageQueue *pMsgQueue, int weight)
{
	if (pMsgQueue == NULL) {
		pMsgQueue = m_pMqMgr->Pop();
		if (pMsgQueue == NULL) {
			return NULL;
		}
	}

	uint32_t handle = pMsgQueue->GetHandle();
	SNContextPtr pContext = m_pHandleMgr->FindContext(handle);
	if (!pContext) {
		struct drop_t d = { handle };
		pMsgQueue->Release(drop_message, &d);
		delete pMsgQueue;
		return m_pMqMgr->Pop();
	}

	int i, n = 1;
	sn_message msg;
	for (i = 0; i < n; i++) {
		if (pMsgQueue->Pop(&msg)) {
			assert(!pMsgQueue->IsInGlobal());
			return m_pMqMgr->Pop();
		}
		else if (i == 0 && weight >= 0) {
			n = pMsgQueue->Length();
			n >>= weight;
		}
		int overload = pMsgQueue->Overload();
		if (overload) {
			skynet_error(pContext.get(), "May overload, message queue length = %d", overload);
		}

		pMonitor->Trigger(msg.source, handle);
		pContext->DispatchMessage(&msg);
		pMonitor->Trigger(0, 0);
	}

	assert(pMsgQueue->IsInGlobal());
	assert(pMsgQueue == pContext->MessageQueue());
	pContext.reset();

	SNMessageQueue *nq = m_pMqMgr->Pop();
	if (nq) {
		// If global mq is not empty , push q back, and return next queue (nq)
		// Else (global mq is empty or block, don't push q back, and return q again (for next dispatch)
		m_pMqMgr->Push(pMsgQueue);
		pMsgQueue = nq;
	}
	return pMsgQueue;
}

void SNServer::Release()
{
	skynet_harbor_exit();
	skynet_timer_exit();

	m_pHandleMgr->LoopAll([](SNContextPtr pContext){
		SNMessageQueue *q = pContext->MessageQueue();
		struct drop_t d = { q->GetHandle() };
		q->Release(drop_message, &d);
		q->ForceInGlobal();
	});

	m_pHandleMgr->RemoveAll();
	delete m_pHandleMgr;
	m_pHandleMgr = NULL;

	m_pModuleMgr->Free();
	delete m_pModuleMgr;
	m_pModuleMgr = NULL;

	skynet_socket_free();

	delete m_pMqMgr;
	m_pMqMgr = NULL;
}

int SNServer::ContextPush(uint32_t handle, sn_message *msg)
{
	SNContextPtr pContext = m_pHandleMgr->FindContext(handle);
	if (!pContext) {
		return -1;
	}
	pContext->Push(msg);
	return 0;
}

void SNServer::PushGlobal(SNMessageQueue *pMsgQueue)
{
	m_pMqMgr->Push(pMsgQueue);
}

static void _filter_args(SNContext *context, int type, int *session, void **data, size_t *sz)
{
	int needcopy = !(type & PTYPE_TAG_DONTCOPY);
	int allocsession = type & PTYPE_TAG_ALLOCSESSION;
	type &= 0xff;

	if (allocsession) {
		assert(*session == 0);
		*session = context->NewSession();
	}

	if (needcopy && *data) {
		char *msg = (char *)sn_malloc(*sz + 1);
		memcpy(msg, *data, *sz);
		msg[*sz] = '\0';
		*data = msg;
	}

	*sz |= type << HANDLE_REMOTE_SHIFT;
}

//////////////////////////////////////////////////////////////////////////
// 全局函数
static void copy_name(char name[GLOBALNAME_LENGTH], const char * addr)
{
	int i;
	for (i = 0; i < GLOBALNAME_LENGTH && addr[i]; i++) {
		name[i] = addr[i];
	}
	for (; i < GLOBALNAME_LENGTH; i++) {
		name[i] = '\0';
	}
}

int skynet_send(skynet_context *context, uint32_t source, uint32_t destination, int type, int session, void * data, size_t sz)
{
	if ((sz & HANDLE_MASK) != sz) {
		skynet_error(context, "The message to %x is too large (sz = %lu)", destination, sz);
		sn_free(data);
		return -1;
	}

	_filter_args(context, type, &session, (void **)&data, &sz);

	if (source == 0) {
		source = context->Handle();
	}

	if (destination == 0) {
		return session;
	}

	if (skynet_harbor_message_isremote(destination)) {
		remote_message *rmsg = (remote_message *)sn_malloc(sizeof(*rmsg));
		rmsg->destination.handle = destination;
		rmsg->message = data;
		rmsg->sz = sz;
		skynet_harbor_send(rmsg, source, session);
	}
	else {
		sn_message smsg;
		smsg.source = source;
		smsg.session = session;
		smsg.data = data;
		smsg.sz = sz;

		if (SNServer::Get()->ContextPush(destination, &smsg)) {
			sn_free(data);
			return -1;
		}
	}
	return session;
}

int skynet_sendname(skynet_context *context, uint32_t source, const char * addr, int type, int session, void * data, size_t sz)
{
	if (source == 0) {
		source = context->handle;
	}
	uint32_t des = 0;
	if (addr[0] == ':') {
		des = strtoul(addr + 1, NULL, 16);
	}
	else if (addr[0] == '.') {
		des = SNServer::Get()->GetHandle().FindName(addr + 1);
		if (des == 0) {
			if (type & PTYPE_TAG_DONTCOPY) {
				sn_free(data);
			}
			return -1;
		}
	}
	else {
		_filter_args(context, type, &session, (void **)&data, &sz);

		remote_message * rmsg = (remote_message *)sn_malloc(sizeof(*rmsg));
		copy_name(rmsg->destination.name, addr);
		rmsg->destination.handle = 0;
		rmsg->message = data;
		rmsg->sz = sz;

		skynet_harbor_send(rmsg, source, session);
		return session;
	}

	return skynet_send(context, source, des, type, session, data, sz);
}

void skynet_callback(skynet_context *context, void *ud, skynet_cb cb)
{
	context->SetCallback(cb, ud);
}

//////////////////////////////////////////////////////////////////////////
// skynet command
struct command_func
{
	const char *name;
	const char *(*func)(skynet_context *context, const char *param);
};

static const char *cmd_timeout(skynet_context *context, const char *param)
{
	char *session_ptr = NULL;
	int ti = strtol(param, &session_ptr, 10);
	int session = context->NewSession();
	skynet_timeout(context->handle, ti, session);
	sn_sprintf(context->result, sizeof(context->result), "%d", session);
	return context->result;
}

static const char *cmd_reg(skynet_context *context, const char *param)
{
	if (param == NULL || param[0] == '\0') {
		sn_sprintf(context->result, sizeof(context->result), ":%x", context->handle);
		return context->result;
	}
	else if (param[0] == '.') {
		return SNServer::Get()->GetHandle().NameHandle(context->handle, param + 1);
	}
	else {
		skynet_error(context, "Can't register global name %s in C", param);
		return NULL;
	}
}

static const char *cmd_query(skynet_context * context, const char *param)
{
	if (param[0] == '.') {
		uint32_t handle = SNServer::Get()->GetHandle().FindName(param + 1);
		if (handle) {
			sn_sprintf(context->result, sizeof(context->result), ":%x", handle);
			return context->result;
		}
	}
	return NULL;
}

static const char *cmd_name(skynet_context *context, const char *param)
{
	int size = strlen(param);
#if _MSC_VER
	string name_, handle_;
	name_.resize(size + 1); 
	handle_.resize(size + 1);
	char *name = (char *)name_.c_str();
	char *handle = (char *)handle_.c_str();
#else
	char name[size + 1];
	char handle[size + 1];
#endif

	sn_sscanf(param, "%s %s", name, handle);
	if (handle[0] != ':') {
		return NULL;
	}
	uint32_t handle_id = strtoul(handle + 1, NULL, 16);
	if (handle_id == 0) {
		return NULL;
	}
	if (name[0] == '.') {
		return SNServer::Get()->GetHandle().NameHandle(handle_id, name + 1);
	}
	else {
		skynet_error(context, "Can't set global name %s in C", name);
	}
	return NULL;
}

static const char *cmd_now(skynet_context *context, const char *param)
{
	uint32_t ti = skynet_gettime();
	sn_sprintf(context->result, sizeof(context->result), "%u", ti);
	return context->result;
}

static void handle_exit(skynet_context *context, uint32_t handle)
{
	if (handle == 0) {
		handle = context->handle;
		skynet_error(context, "KILL self");
	}
	else {
		skynet_error(context, "KILL :%0x", handle);
	}
	if (SNServer::Get()->m_monitor_exit) {
		skynet_send(context, handle, SNServer::Get()->m_monitor_exit, PTYPE_CLIENT, 0, NULL, 0);
	}
	SNContextPtr pContext = SNServer::Get()->GetHandle().Remove(handle);
	if (pContext) {
		pContext->MessageQueue()->ForceInGlobal();
	}
}

static const char *cmd_exit(skynet_context *context, const char *param)
{
	handle_exit(context, 0);
	return NULL;
}

static uint32_t tohandle(skynet_context *context, const char *param)
{
	uint32_t handle = 0;
	if (param[0] == ':') {
		handle = strtoul(param + 1, NULL, 16);
	}
	else if (param[0] == '.') {
		handle = SNServer::Get()->GetHandle().FindName(param + 1);
	}
	else {
		skynet_error(context, "Can't convert %s to handle", param);
	}

	return handle;
}

static const char *cmd_kill(skynet_context *context, const char *param)
{
	uint32_t handle = tohandle(context, param);
	if (handle) {
		handle_exit(context, handle);
	}
	return NULL;
}

static const char *cmd_launch(skynet_context *context, const char *param)
{
	size_t sz = strlen(param);
#if _MSC_VER
	string tmp_;
	tmp_.resize(sz + 1);
	char *tmp = (char *)tmp_.c_str();
#else
	char tmp[sz + 1];
#endif
	sn_strcpy(tmp, sz + 1, param);
	char *args = tmp;
	char *mod = strsep(&args, " \t\r\n");
	args = strsep(&args, "\r\n");
	SNContextPtr inst = SNServer::Get()->NewContext(mod, args);
	if (inst == NULL) {
		return NULL;
	}
	else {
		id_to_hex(context->result, inst->handle);
		return context->result;
	}
}

static const char *cmd_getenv(skynet_context *context, const char *param)
{
	return skynet_getenv(param);
}

static const char *cmd_setenv(skynet_context *context, const char *param)
{
	size_t sz = strlen(param);
#if _MSC_VER
	string key_;
	key_.resize(sz + 1);
	char *key = (char *)key_.c_str();
#else
	char key[sz + 1];
#endif
	int i;
	for (i = 0; param[i] != ' ' && param[i]; i++) {
		key[i] = param[i];
	}
	if (param[i] == '\0')
		return NULL;

	key[i] = '\0';
	param += i + 1;

	skynet_setenv(key, param);
	return NULL;
}

static const char *cmd_starttime(skynet_context *context, const char *param)
{
	uint32_t sec = skynet_gettime_fixsec();
	sn_sprintf(context->result, sizeof(context->result), "%u", sec);
	return context->result;
}

static const char *cmd_endless(skynet_context *context, const char *param)
{
	if (context->endless) {
		sn_strcpy(context->result, sizeof(context->result), "1");
		context->endless = false;
		return context->result;
	}
	return NULL;
}

static const char *cmd_abort(skynet_context *context, const char *param)
{
	SNServer::Get()->Exit();
	return NULL;
}

static const char *cmd_monitor(skynet_context *context, const char *param)
{
	uint32_t handle = 0;
	if (param == NULL || param[0] == '\0') {
		if (SNServer::Get()->m_monitor_exit) {
			// return current monitor serivce
			sn_sprintf(context->result, sizeof(context->result), ":%x", SNServer::Get()->m_monitor_exit);
			return context->result;
		}
		return NULL;
	}
	else {
		handle = tohandle(context, param);
	}
	SNServer::Get()->m_monitor_exit = handle;
	return NULL;
}

static const char *cmd_mqlen(skynet_context *context, const char *param)
{
	int len = context->MessageQueue()->Length();
	sn_sprintf(context->result, sizeof(context->result), "%d", len);
	return context->result;
}

static const char *cmd_logon(skynet_context *context, const char *param)
{
	uint32_t handle = tohandle(context, param);
	if (handle == 0) {
		return NULL;
	}

	SNContextPtr ctx = SNServer::Get()->GetHandle().FindContext(handle);
	if (!ctx) {
		return NULL;
	}

	FILE *f = NULL;
	FILE *lastf = ctx->m_logfile;
	if (lastf == NULL) {
		f = sn_log_open(context, handle);
		if (f) {
			FILE *o = NULL;
			if (!ctx->m_logfile.compare_exchange_strong(o, f)) {
				// logfile opens in other thread, close this one.
				fclose(f);
			}
		}
	}
	return NULL;
}

static const char *cmd_logoff(skynet_context *context, const char *param)
{
	uint32_t handle = tohandle(context, param);
	if (handle == 0)
		return NULL;
	SNContextPtr ctx = SNServer::Get()->GetHandle().FindContext(handle);
	if (!ctx)
		return NULL;
	FILE *f = ctx->m_logfile;
	if (f) {
		// logfile may close in other thread
		if (ctx->m_logfile.compare_exchange_strong(f, NULL)) {
			sn_log_close(context, f, handle);
		}
	}
	return NULL;
}

static struct command_func cmd_funcs[] = {
	{ "TIMEOUT", cmd_timeout },
	{ "REG", cmd_reg },
	{ "QUERY", cmd_query },
	{ "NAME", cmd_name },
	{ "NOW", cmd_now },
	{ "EXIT", cmd_exit },
	{ "KILL", cmd_kill },
	{ "LAUNCH", cmd_launch },
	{ "GETENV", cmd_getenv },
	{ "SETENV", cmd_setenv },
	{ "STARTTIME", cmd_starttime },
	{ "ENDLESS", cmd_endless },
	{ "ABORT", cmd_abort },
	{ "MONITOR", cmd_monitor },
	{ "MQLEN", cmd_mqlen },
	{ "LOGON", cmd_logon },
	{ "LOGOFF", cmd_logoff },
	{ NULL, NULL },
};

const char *skynet_command(skynet_context *context, const char *cmd, const char *param)
{
	command_func * method = &cmd_funcs[0];
	while (method->name) {
		if (strcmp(cmd, method->name) == 0) {
			return method->func(context, param);
		}
		++method;
	}

	return NULL;
}

uint32_t skynet_queryname(struct skynet_context * context, const char * name)
{
	switch (name[0]) {
	case ':':
		return strtoul(name + 1, NULL, 16);
	case '.':
		return SNServer::Get()->GetHandle().FindName(name + 1);
	}
	skynet_error(context, "Don't support query global name %s", name);
	return 0;
}

int skynet_isremote(struct skynet_context * ctx, uint32_t handle, int * harbor)
{
	int ret = skynet_harbor_message_isremote(handle);
	if (harbor) {
		*harbor = (int)(handle >> HANDLE_REMOTE_SHIFT);
	}
	return ret;
}

uint32_t skynet_current_handle(void)
{
	if (SNServer::Get()->m_init) {
		void * handle = (void *)*SNServer::Get()->m_handle_key;
		return (uint32_t)(uintptr_t)handle;
	}
	else {
		uintptr_t v = (uint32_t)(-THREAD_MAIN);
		return v;
	}
}
