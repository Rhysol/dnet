#pragma once
#include <time.h>
#include "epoll_event_manager.h"
#include "epoll_event_handler.h"

namespace std {
    class thread;
}

struct IOEvent;


class IOThread
{
public:
    IOThread();
    virtual ~IOThread();

    virtual bool Init(uint16_t thread_id, const CreateNetPacketFunc &create_packet_func, 
        const EpollEventHandler::OutputIOEventPipe &output_event_pipe);

    void Start();
    void Stop();
    void Join();
    virtual void Update();

    virtual void RegisterConnectionFd(int32_t fd);

    inline uint16_t GetThreadId() { return m_thread_id; }

protected:
    uint16_t m_thread_id;
    std::thread *m_thread = nullptr;

    EpollEventManager m_epoll_event_manager;
    EpollEventHandler m_epoll_event_handler;

    bool m_keep_alive = true;

    timespec m_sleep_interval;
};