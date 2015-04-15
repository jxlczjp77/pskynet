#ifndef _SN_MONITOR_H_
#define _SN_MONITOR_H_
#include "sn_common.h"

class SNMonitor
{
public:
	SNMonitor();
	~SNMonitor();

	void Check();
	void Trigger(uint32_t source, uint32_t destination);

private:
	std::atomic_uint32_t m_version;
	int m_check_version;
	uint32_t m_source;
	uint32_t m_destination;
};

#endif
