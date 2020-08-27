#include "listener.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <cstring>
#include "net_config.h"
#include "logger.h"

using namespace dnet;

Listener::Listener()
{
}

Listener::~Listener()
{
	if (m_listener_fd != -1)
	{
		close(m_listener_fd);
	}
}

bool Listener::StartListen(const NetConfig *net_config, const OutputIOEventPipe &output_event_pipe)
{
	if (m_listener_fd != -1) return true;

    m_output_io_event_pipe = output_event_pipe;

	m_net_config = net_config;
	sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(sockaddr_in));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(m_net_config->listen_port);
	if (inet_aton(m_net_config->listen_ip.c_str(), &server_addr.sin_addr) == 0)
	{
		LOGE("bind address is invalid");
		return false;
	}
	m_listener_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
	if (m_listener_fd == -1)
	{
		LOGE("create listen socket failed");
		return false;
	}
	if (bind(m_listener_fd, (sockaddr *)&server_addr, sizeof(sockaddr)) == -1)
	{
		LOGE("bind listener fd failed");
		return false;
	}
	if (listen(m_listener_fd, global_config.listen_queue_max_num) == -1)
	{
		LOGE("listen failed!");
		return false;
	}

	return true;
}
 
void Listener::OnAccept()
{
    sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(sockaddr_in));
    socklen_t len = sizeof(sockaddr);
	int client_fd = accept4(m_listener_fd, (sockaddr *)&client_addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
	if (client_fd == -1)
	{
		LOGW("accept client failed! errno: {}", errno);
	}
	else
	{
        AcceptConnectionEvent *event = new AcceptConnectionEvent; //在net_manager内被删除
        event->connection_fd = client_fd;
        event->remote_ip = inet_ntoa(client_addr.sin_addr);
        event->remote_port = ntohs(client_addr.sin_port);
        m_output_io_event_pipe(event);
	}
}