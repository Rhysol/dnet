#include "listener_thread.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <cstring>
#include "net_config.h"
#include "logger.h"

using namespace dnet;

ListenerThread::ListenerThread()
{

}

ListenerThread::~ListenerThread()
{
	if (m_listener_fd != -1)
	{
		close(m_listener_fd);
	}
}

bool ListenerThread::Init(uint16_t thread_id, const NetConfig *net_config)
{
    if(!IOThread::Init(thread_id, net_config)) return false;
    if (!StartListen()) return false;

    //把listener_fd注册到epoll
    if (!EpollCtl(m_listener_fd, EPOLLIN, EPOLL_CTL_ADD))
    {
        LOGE("register listen event to epoll failed!");
        return false;
    }
    return true;
}

bool ListenerThread::StartListen()
{
	if (m_listener_fd != -1) return true;

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
	linger linger_val;
	linger_val.l_onoff = 1;
	linger_val.l_linger = 3;
	if (setsockopt(m_listener_fd, SOL_SOCKET, SO_LINGER, (void *)&linger_val, sizeof(linger)) == -1)
	{
		LOGE("set listener_fd linger failed, errno: {}", errno);
		return false;
	}
	if (bind(m_listener_fd, (sockaddr *)&server_addr, sizeof(sockaddr)) == -1)
	{
		LOGE("bind listener fd failed");
		return false;
	}
	if (listen(m_listener_fd, m_net_config->listen_queue_max_num) == -1)
	{
		LOGE("listen failed!");
		return false;
	}

	return true;
}

void ListenerThread::HandleEpollEvent(const epoll_event &ev)
{
    if (ev.data.fd == m_listener_fd)
    {
        OnAccept();
    }
    else
    {
        IOThread::HandleEpollEvent(ev);
    }
}

void ListenerThread::OnAccept()
{
    sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(sockaddr_in));
    socklen_t len = sizeof(sockaddr);
	int client_fd = accept4(m_listener_fd, (sockaddr *)&client_addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
	if (client_fd == -1)
	{
		LOGW("accept client failed! errno: {}", errno);
		return;
	}
	linger linger_val;
	linger_val.l_onoff = 1;
	linger_val.l_linger = 3;
	if (setsockopt(client_fd, SOL_SOCKET, SO_LINGER, (void *)&linger_val, sizeof(linger)) == -1)
	{
		LOGE("set client_fd linger failed, errno: {}", errno);
		return;
	}
	int32_t reuse_addr = 1;
	if (setsockopt(client_fd, SOL_SOCKET, SO_REUSEADDR, (void *)&reuse_addr, sizeof(int32_t)) == -1)
	{
		LOGE("set client_fd SO_REUSEADDR failed, errno: {}", errno);
		return;
	}
	int32_t nodelay = 1;
	if (setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, (void *)&nodelay, sizeof(int32_t)) == -1)
	{
		LOGE("set client_fd TCP_NODELAY failed, errno: {}", errno);
		return;
	}

    AcceptConnectionEvent *event = new AcceptConnectionEvent; //在net_manager内被删除
    event->connection_fd = client_fd;
    event->remote_ip = inet_ntoa(client_addr.sin_addr);
    event->remote_port = ntohs(client_addr.sin_port);
    Pass2MainThread(event);
}