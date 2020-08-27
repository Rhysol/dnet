#pragma once

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include "io_event.h"

namespace dnet
{

class Listener 
{
public:
	Listener();
	~Listener();

	bool StartListen(const std::string &listen_ip, uint16_t listen_port, const OutputIOEventPipe &output_event_pipe);

	inline int32_t GetListenerFd() { return m_listener_fd; }

	void OnAccept();

private:
	static uint16_t listen_queue_max_count;

    OutputIOEventPipe m_output_io_event_pipe;

	std::string m_listen_ip;
	uint16_t m_listen_port;
	int32_t m_listener_fd = -1;
};

}