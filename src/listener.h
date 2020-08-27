#pragma once

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include "io_event.h"

namespace dnet
{

struct NetConfig;

class Listener 
{
public:
	Listener();
	~Listener();

	bool StartListen(const NetConfig *net_config, const OutputIOEventPipe &output_event_pipe);

	inline int32_t GetListenerFd() { return m_listener_fd; }

	void OnAccept();

private:
	const NetConfig *m_net_config = nullptr;

    OutputIOEventPipe m_output_io_event_pipe;

	int32_t m_listener_fd = -1;
};

}