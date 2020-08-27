#include "epoll_event_manager.h"
#include "net_config.h"
#include "logger.h"

using namespace dnet;

EpollEventManager::EpollEventManager()
{
}

EpollEventManager::~EpollEventManager()
{
	close(m_epoll_fd);
	if (m_events != nullptr)
	{
		delete[] m_events;
	}
}

bool EpollEventManager::Init(const HandleEventFunc &func, uint32_t max_events_num)
{
	if (max_events_num == 0 || !func) return false;
	m_handle_event_func = func;
	m_epoll_fd = epoll_create1(EPOLL_CLOEXEC);
	m_events = new epoll_event[global_config.epoll_max_event_num];
	return true;
}

uint32_t EpollEventManager::Update()
{
	int event_num = epoll_wait(m_epoll_fd, m_events, global_config.epoll_max_event_num, 0);
	if (event_num == -1)
	{
		LOGE("epoll_wait failed!");
		return 0;
	}
	for (int i = 0; i < event_num; i++)
	{
		m_handle_event_func(m_events[i]);
	}
	return event_num;
}

bool EpollEventManager::MonitorFd(int32_t fd, epoll_event &event)
{
	int operation = EPOLL_CTL_MOD;
	if (m_monitoring_fd.find(fd) == m_monitoring_fd.end())
	{
		m_monitoring_fd.emplace(fd);
		operation = EPOLL_CTL_ADD;
	}

	if (epoll_ctl(m_epoll_fd, operation, fd, &event) != 0)
	{
		LOGE("epoll_ctl failed! operation: {}, errno: {}", operation, errno);
		return false;
	}
	return true;
}

void EpollEventManager::StopMonitorFd(int32_t fd)
{
	m_monitoring_fd.erase(fd);
	epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, fd, NULL);
}

bool EpollEventManager::IsFdMonitored(int32_t fd)
{
	return m_monitoring_fd.find(fd) != m_monitoring_fd.end();
}