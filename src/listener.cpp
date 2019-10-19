#include "listener.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <cstring>

uint16_t Listener::listen_queue_max_count = 1000;

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

bool Listener::StartListen(const std::string &listen_ip, uint16_t listen_port)
{
	if (m_listener_fd != -1) return true;

	m_listen_ip = listen_ip;
	m_listen_port = listen_port;
	sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(sockaddr_in));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(m_listen_port);
	if (inet_aton(m_listen_ip.c_str(), &server_addr.sin_addr) == 0)
	{
		std::cout << "bind address is invalid" << std::endl;
		return false;
	}
	m_listener_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
	if (m_listener_fd == -1)
	{
		std::cout << "create listen socket failed" << std::endl;
		return false;
	}
	if (bind(m_listener_fd, (sockaddr *)&server_addr, sizeof(sockaddr)) == -1)
	{
		std::cout << "bind listener fd failed" << std::endl;
		return false;
	}
	if (listen(m_listener_fd, listen_queue_max_count) == -1)
	{
		std::cout << "listen failed" << std::endl;
		return false;
	}

	return true;
}
 
// void Listener::HandleIO(const epoll_event &ev)
// {
// 	char buffer[65536];
// 	if (ev.events & EPOLLOUT)
// 	{
// 		std::cout << "write event" << std::endl;
// 	}
// 	if (ev.events & EPOLLIN)
// 	{
// 		memset(buffer, 0, 1024);
// 		int read_len = read(ev.data.fd, buffer, 1024);
// 		std::cout << "read_len[" << read_len << "], " << buffer << std::endl;
// 		if (read_len == 0)
// 		{
// 			epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, ev.data.fd, NULL);
// 			return;
// 		}

// 		if (read_len != -1)
// 		{
// 			int write_len = write(ev.data.fd, buffer, read_len);
// 			std::cout << "write_len[" << write_len << "]" << std::endl;
// 			if (write_len == -1 && errno == EAGAIN)
// 			{
// 				std::cout << "write failed EAGAIN" << std::endl;
// 			}
// 		}
// 	}

// 	memset(&m_event_tmp, 0, sizeof(m_event_tmp));
// 	m_event_tmp.events = EPOLLIN | EPOLLOUT | EPOLLET;
// 	m_event_tmp.data.fd = ev.data.fd;
// 	epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, ev.data.fd, &m_event_tmp);
// }