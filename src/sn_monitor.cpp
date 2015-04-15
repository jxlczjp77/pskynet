#include "sn_monitor.h"
#include "skynet.h"
#include "sn_server.h"

SNMonitor::SNMonitor()
{
	m_version = 0;
	m_check_version = 0;
	m_source = 0;
	m_destination = 0;
}

SNMonitor::~SNMonitor()
{

}

void SNMonitor::Check()
{
	if (m_version == m_check_version) {
		if (m_destination) {
			SNServer::Get()->Endless(m_destination);
			skynet_error(NULL, "A message from [ :%08x ] to [ :%08x ] maybe in an endless loop (version = %d)", m_source, m_destination, m_version);
		}
	}
	else {
		m_check_version = m_version;
	}
}

void SNMonitor::Trigger(uint32_t source, uint32_t destination)
{
	m_source = source;
	m_destination = destination;
	m_version++;
}
