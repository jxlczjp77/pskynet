#ifndef _SN_HANDLE_H_
#define _SN_HANDLE_H_
#include "sn_common.h"
#include "sn_harbor.h"

#define INVALID_HANDLE 0

class SNHandleMgr
{
public:
	SNHandleMgr(int harbor);
	~SNHandleMgr();

	uint32_t Register(SNContextPtr &pContext);
	SNContextPtr FindContext(uint32_t handle);

	// 释放指定Handle的位置
	SNContextPtr Remove(uint32_t handle);

	// 释放所有的Handle
	void RemoveAll();

	uint32_t FindName(const char *name);
	const char *NameHandle(uint32_t handle, const char *name);

	void LoopAll(const std::function<void(SNContextPtr)> &fun);

private:
	uint32_t m_harbor;
	uint32_t m_handle_index;
	RWMutex m_mutex;
	std::unordered_map<uint32_t, SNContextPtr> m_slot;
	std::unordered_map<string, uint32_t> m_nameMap;
};

#endif
