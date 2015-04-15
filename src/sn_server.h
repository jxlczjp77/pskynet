#ifndef _SN_SERVER_H_
#define _SN_SERVER_H_
#include "sn_common.h"
#include "skynet.h"
#include "sn_mq.h"
#include "sn_module.h"
#include "sn_handle.h"
#include "sn_monitor.h"

#ifdef CALLING_CHECK
#define CHECKCALLING_BEGIN(ctx) assert(ctx->calling.exchange(1) == 0);
#define CHECKCALLING_END(ctx) ctx->calling = 0;
#define CHECKCALLING_INIT(ctx) ctx->calling = 0;
#define CHECKCALLING_DECL std::atomic_int32_t calling;
#else
#define CHECKCALLING_BEGIN(ctx)
#define CHECKCALLING_END(ctx)
#define CHECKCALLING_INIT(ctx)
#define CHECKCALLING_DECL
#endif

struct SNContext : public std::enable_shared_from_this<SNContext>
{
	friend class SNServer;
public:
	SNContext(SNModule *mod, void *inst);
	~SNContext();
	bool Init(SNMessageQueue *queue, uint32_t hanle, const char *parm);
	SNMessageQueue *MessageQueue() { return m_queue; }
	uint32_t Handle() { return handle; }
	void Endless() { endless = true; }
	void DispatchMessage(sn_message *msg);

	void Push(sn_message *msg);
	void Push(void * msg, size_t sz, uint32_t source, int type, int session);

	uint32_t NewSession();

	void SetCallback(skynet_cb cb, void *ud) {
		m_cb_ud = ud;
		m_cb = cb;
	}

private:
	void DispatchAll();

public:
	void *m_instance;
	SNModule *m_mod;
	void *m_cb_ud;
	skynet_cb m_cb;
	SNMessageQueue *m_queue;
	std::atomic<FILE *> m_logfile;
	char result[32];
	uint32_t handle;
	int m_session_id;
	bool m_init;
	bool endless;

	CHECKCALLING_DECL
};

class SNServer
{
public:
	static SNServer *Get();

	SNServer(int harbor, const char *module_path);
	~SNServer();

	void ThreadInit(uint32_t threadType);

	SNContextPtr NewContext(const char *name, const char *param);
	void DispatchAll(SNContextPtr &pContext);	// for skynet_error output before exit
	SNMessageQueue *DispatchMessageQueue(SNMonitor *, SNMessageQueue *, int weight);	// return next queue
	void Endless(uint32_t handle);	// for monitor
	uint32_t Total() { return m_total; }

	void PushGlobal(SNMessageQueue *pMsgQueue);
	SNHandleMgr &GetHandle() { return *m_pHandleMgr; }
	int ContextPush(uint32_t handle, sn_message *msg);

	void Release();

	void Exit() { m_bExit = true; }
	bool IsExit() const { return m_bExit; }

private:
	void DeleteContext(SNContext *pContext);

public:
	bool m_bExit;

	std::atomic_uint32_t m_total;
	int m_init;
	uint32_t m_monitor_exit;
	SNModuleMgr *m_pModuleMgr;
	SNHandleMgr *m_pHandleMgr;
	SNMessageQueueMgr *m_pMqMgr;
	boost::thread_specific_ptr<uint32_t> m_handle_key;
};

#endif
