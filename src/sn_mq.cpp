#include "sn_mq.h"
#include "sn_server.h"

#define DEFAULT_QUEUE_SIZE 64
#define MAX_GLOBAL_MQ 65534 // boost::lockfree::fixed_sized<true>最大只能到这么大

#define MQ_OVERLOAD 1024

//////////////////////////////////////////////////////////////////////////
// SNMessageQueue
SNMessageQueue::SNMessageQueue(uint32_t handle)
{
	m_handle = handle;
	m_mutex.v_ = 0;
	m_overload = 0;
	m_overload_threshold = MQ_OVERLOAD;
	next = NULL;

	// 创建的时候故意设置为true，保证整个SNContext初始化过程结束前
	// 不会入全局调度队列
	m_bInGlobal = true;
}

SNMessageQueue::~SNMessageQueue()
{
	
}

int SNMessageQueue::Pop(sn_message *message, bool removeGlobal)
{
	SpinLock::scoped_lock lock(m_mutex);

	int ret = 1;
	if (m_queue.empty()) {
		// 队列空，重置overload_threshold
		m_overload_threshold = MQ_OVERLOAD;
	}
	else {
		ret = 0;
		*message = m_queue.front();
		m_queue.pop();
		int length = (int)m_queue.size();
		while (length > m_overload_threshold) {
			m_overload = length;
			m_overload_threshold *= 2;
		}
	}

	if (removeGlobal && ret) {
		m_bInGlobal = false;
	}

	return ret;
}

void SNMessageQueue::Push(const sn_message *message)
{
	SpinLock::scoped_lock lock(m_mutex);
	m_queue.push(*message);

	if (!m_bInGlobal) {
		m_bInGlobal = true;
		SNServer::Get()->PushGlobal(this);
	}
}

void SNMessageQueue::ForceInGlobal()
{
	SpinLock::scoped_lock lock(m_mutex);
	if (!m_bInGlobal) {
		m_bInGlobal = true;
		SNServer::Get()->PushGlobal(this);
	}
}

int SNMessageQueue::Length()
{
	SpinLock::scoped_lock lock(m_mutex);
	return (int)m_queue.size();
}

int SNMessageQueue::Overload()
{
	if (m_overload) {
		int overload = m_overload;
		m_overload = 0;
		return overload;
	}
	return 0;
}

uint32_t SNMessageQueue::GetHandle()
{
	return m_handle;
}

void SNMessageQueue::Release(message_drop drop_func, void *ud)
{
	_drop_queue(drop_func, ud);
}

void SNMessageQueue::_drop_queue(message_drop drop_func, void *ud)
{
	sn_message msg;
	while (!Pop(&msg, false)) {
		drop_func(&msg, ud);
	}
}

//////////////////////////////////////////////////////////////////////////
// SNMessageQueueMgr
SNMessageQueueMgr::SNMessageQueueMgr()
#ifndef _DEBUG
	: m_queue(MAX_GLOBAL_MQ)
#endif
{
	list = NULL;
}

SNMessageQueueMgr::~SNMessageQueueMgr()
{
	SNMessageQueue *q = NULL;
	while (q = Pop()) {
		delete q;
	}
}

void SNMessageQueueMgr::Push(SNMessageQueue *queue)
{
#ifdef _DEBUG
	std::unique_lock<std::mutex> l(m_mutex);
	m_queue.push(queue);
#else
	assert(queue->m_bInGlobal == true);
	if (!m_queue.push(queue)) {
		assert(queue->next == NULL);
		while (!list.compare_exchange_strong(queue->next, queue)){
			;
		}
	}
#endif
	
}

SNMessageQueue *SNMessageQueueMgr::Pop()
{
#ifdef _DEBUG
	std::unique_lock<std::mutex> l(m_mutex);
	if (m_queue.empty()) {
		return NULL;
	}
	SNMessageQueue *ret = m_queue.front();
	m_queue.pop();
	return ret;
#else
	SNMessageQueue *ret = NULL;
	if (m_queue.pop(ret)) {
		SNMessageQueue *l = list;
		if (l) {
			SNMessageQueue *newhead = l->next;
			if (list.compare_exchange_strong(l, newhead)) {
				// try load list only once, if success , push it back to the queue.
				l->next = NULL;
				Push(l);
			}
		}
		return ret;
	}
	return NULL;
#endif
	
}
