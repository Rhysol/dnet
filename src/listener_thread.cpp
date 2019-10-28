#include "listener_thread.h"
#include <cstring>


ListenerThread::ListenerThread()
{

}

ListenerThread::~ListenerThread()
{

}

bool ListenerThread::Init(uint16_t thread_id, const std::string &listen_ip, uint16_t listen_port,
    const CreateNetPacketFunc &create_packet_func, const EpollEventHandler::OutputIOEventPipe &output_event_pipe) 
{
    if (!m_listener.StartListen(listen_ip, listen_port)) return false;

    m_thread_id = thread_id;
    m_sleep_interval.tv_sec = 0;
    m_sleep_interval.tv_nsec = 1000 * 1000; // 1ms
    m_epoll_event_handler.Init(create_packet_func, output_event_pipe);
    if (!m_epoll_event_manager.Init(std::bind(&EpollEventHandler::HandleListenEvent, m_epoll_event_handler, 
        std::placeholders::_1, m_listener.GetListenerFd())))
    { 
        std::cout << "listener epoll init failed" << std::endl;
        return false;
    }

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
}