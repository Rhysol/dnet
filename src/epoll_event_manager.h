#pragma once

#include <sys/epoll.h>
#include <unistd.h>
#include <iostream>
#include <functional>
#include <set>


class EpollEventManager
{
public:
	typedef std::function<void(const epoll_event &)> HandleEventFunc;
	EpollEventManager();
	~EpollEventManager();

	bool Init(const HandleEventFunc &func, uint32_t max_events_num = 10);

	uint32_t Update();

	//使epoll监听fd的事件，如果fd已经被监听，则更新监听事件
	bool MonitorFd(int32_t fd, epoll_event &event);

	//将fd从epoll的监听列表中移除
	void StopMonitorFd(int32_t fd);

private:
	int m_epoll_fd;
	epoll_event *m_events = nullptr;
	uint32_t m_max_events_num;
	HandleEventFunc m_handle_event_func;
	std::set<int32_t> m_monitoring_fd;
};
