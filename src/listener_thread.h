#pragma once
#include "io_thread.h"
#include "listener.h"


class ListenerThread : public IOThread
{
public:
    ListenerThread();
    ~ListenerThread();

    typedef std::function<void (int32_t listener_fd, uint16_t thread_id)> AcceptConnectionFunc;
    bool Init(uint16_t thread_id, const std::string &listen_ip, uint16_t listen_port, 
        const AcceptConnectionFunc &accept_func, const ReadFunc &read_func);

private:
    virtual void HandleEpollEvent(const epoll_event &ev) override;

private:
    Listener m_listener;
    AcceptConnectionFunc m_accept_connection_func;
};