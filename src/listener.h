#pragma once

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include "epoll_event_manager.h"

class Listener 
{
public:
	Listener();
	~Listener();

	bool StartListen(const std::string &listen_ip, uint16_t listen_port);

	inline int32_t GetListenerFd() { return m_listener_fd; }

private:
	static uint16_t listen_queue_max_count;

	std::string m_listen_ip;
	uint16_t m_listen_port;
	int32_t m_listener_fd = -1;
};