#pragma once
#include "epoll_event_manager.h"
#include <time.h>

namespace std {
    class thread;
}


class IOThread
{
public:
    IOThread();
    virtual ~IOThread();

    typedef std::function<void (int32_t fd, uint16_t thread_id)> ReadFunc;
    virtual bool Init(uint16_t thread_id, const ReadFunc &func);

    void Start();
    void Stop();
    void Join();
    virtual void Update();

    virtual void RegisterConnectionFd(int32_t fd);

    inline uint16_t GetThreadId() { return m_thread_id; }

protected:
    virtual void HandleEpollEvent(const epoll_event &ev);

protected:
    uint16_t m_thread_id;
    std::thread *m_thread = nullptr;

    EpollEventManager m_epoll_event_manager;

    ReadFunc m_read_func;

    bool m_keep_alive = true;

    timespec m_sleep_interval;
};