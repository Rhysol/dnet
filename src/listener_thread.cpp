#include "listener_thread.h"
#include <cstring>
#include "logger.h"

using namespace dnet;

ListenerThread::ListenerThread()
{

}

ListenerThread::~ListenerThread()
{

}

bool ListenerThread::Init(uint16_t thread_id, const std::string &listen_ip, uint16_t listen_port,
    const ReadHandler::CreateNetPacketFunc &create_packet_func, const OutputIOEventPipe &output_event_pipe) 
{
    if(!IOThread::Init(thread_id, create_packet_func, output_event_pipe)) return false;
    if (!m_listener.StartListen(listen_ip, listen_port, output_event_pipe)) return false;

    //把listener_fd注册到epoll
    epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.fd = m_listener.GetListenerFd();
    if (!m_epoll_event_manager.MonitorFd(m_listener.GetListenerFd(), ev))
    {
        LOGE("register listen event to epoll failed!");
        return false;
    }
    return true;
}

void ListenerThread::HandleEpollEvent(const epoll_event &ev)
{
    if (ev.data.fd == m_listener.GetListenerFd())
    {
        m_listener.OnAccept();
    }
    else
    {
        IOThread::HandleEpollEvent(ev);
    }
}