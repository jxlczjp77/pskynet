#include "sn_handle.h"
#include "sn_server.h"

SNHandleMgr::SNHandleMgr(int harbor)
{
	m_handle_index = 1;
	m_harbor = (uint32_t)(harbor & 0xff) << HANDLE_REMOTE_SHIFT;
}

SNHandleMgr::~SNHandleMgr()
{

}

uint32_t SNHandleMgr::Register(SNContextPtr &pContext)
{
	WLock lock(m_mutex);
	for (int i = 0; ; ++i) {
		uint32_t handle = (m_handle_index + i) & HANDLE_MASK;
		if (m_slot.find(handle) == m_slot.end()) {
			m_slot[handle] = pContext;
			m_handle_index = handle + 1;
			handle |= m_harbor;
			return handle;
		}
	}
	return INVALID_HANDLE;
}

SNContextPtr SNHandleMgr::Remove(uint32_t handle)
{
	SNContextPtr ret;
	handle &= HANDLE_MASK;

	WLock lock(m_mutex);
	auto it = m_slot.find(handle);
	if (it != m_slot.end() && it->second->Handle() == handle) {
		ret = it->second;
		m_slot.erase(it);
		for (auto it1 = m_nameMap.begin(); it1 != m_nameMap.end();) {
			if (it1->second == handle) {
				it1 = m_nameMap.erase(it1);
			}
			else {
				++it1;
			}
		}
	}
	return ret;
}

SNContextPtr SNHandleMgr::FindContext(uint32_t handle)
{
	handle &= HANDLE_MASK;
	RLock lock(m_mutex);
	auto it = m_slot.find(handle);
	if (it != m_slot.end()) {
		return it->second;
	}
	return SNContextPtr();
}

void SNHandleMgr::RemoveAll()
{
	WLock lock(m_mutex);
	m_slot.clear();
	m_nameMap.clear();
}

void SNHandleMgr::LoopAll(const std::function<void(SNContextPtr)> &fun)
{
	RLock lock(m_mutex);
	for (auto it = m_slot.begin(); it != m_slot.end(); ++it) {
		fun(it->second);
	}
}

uint32_t SNHandleMgr::FindName(const char *name)
{
	RLock lock(m_mutex);
	auto it = m_nameMap.find(name);
	if (it != m_nameMap.end()) {
		return it->second;
	}
	return INVALID_HANDLE;
}

const char *SNHandleMgr::NameHandle(uint32_t handle, const char *name)
{
	WLock lock(m_mutex);
	auto it = m_nameMap.find(name);
	if (it != m_nameMap.end()) {
		return NULL;
	}

	auto Ret = m_nameMap.insert(std::make_pair(name, handle));
	assert(Ret.second == true);
	return Ret.first->first.c_str();
}
