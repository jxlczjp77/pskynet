#ifndef _SN_MQ_H_
#define _SN_MQ_H_
#include "sn_common.h"

struct sn_message
{
	uint32_t source;
	int session;
	void * data;
	size_t sz;
};
typedef void(*message_drop)(sn_message *, void *);

class SNMessageQueue
{
	friend class SNMessageQueueMgr;
public:
	SNMessageQueue(uint32_t handle);
	~SNMessageQueue();

	// 0 for success
	int Pop(sn_message *message, bool removeGlobal = true);
	void Push(const sn_message *message);
	void ForceInGlobal();

	int Length();
	int Overload();

	uint32_t GetHandle();
	void Release(message_drop drop_func, void *ud);

	bool IsInGlobal() const { return m_bInGlobal; }

private:
	void _drop_queue(message_drop drop_func, void *ud);

private:
	bool m_bInGlobal;
	int m_overload;
	int m_overload_threshold;
	std::queue<sn_message> m_queue;
	SpinLock m_mutex;
	uint32_t m_handle;

public:
	SNMessageQueue *next;
};

class SNMessageQueueMgr
{
public:
	SNMessageQueueMgr();
	~SNMessageQueueMgr();

	void Push(SNMessageQueue *queue);
	SNMessageQueue *Pop();

private:

#ifdef _DEBUG
	// Debug版本用queue方便调试
	std::mutex m_mutex;
	std::queue<SNMessageQueue *> m_queue;
#else
	boost::lockfree::queue<SNMessageQueue *, boost::lockfree::fixed_sized<true> > m_queue;
#endif // DEBUG

	std::atomic<SNMessageQueue *> list;
};

#endif
