#include "listener_thread.h"
#include <cstring>


ListenerThread::ListenerThread()
{

}

ListenerThread::~ListenerThread()
{

}

bool ListenerThread::Init(uint16_t thread_id, const std::string &listen_ip, uint16_t listen_port, 
    const AcceptConnectionFunc &accept_func, const ReadFunc &read_func) 
{
    if(!IOThread::Init(thread_id, read_func)) return false;
    if (!m_listener.StartListen(listen_ip, listen_port)) return false;

    //把listener_fd注册到epoll
    epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.fd = m_listener.GetListenerFd();
    if (!m_epoll_event_manager.MonitorFd(m_listener.GetListenerFd(), ev))
    {
        std::cout << "register listen event to epoll failed!" << std::endl;
        return false;
    }

    m_accept_connection_func = accept_func;
    return true;
}

void ListenerThread::HandleEpollEvent(const epoll_event &ev)
{
    if (ev.data.fd == m_listener.GetListenerFd())
    {
        m_accept_connection_func(ev.data.fd, m_thread_id);
    }
    else
    {
        IOThread::HandleEpollEvent(ev);
    }
}
