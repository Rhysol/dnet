#pragma once
#include <time.h>
#include "epoll_event_manager.h"
#include "read_handler.h"
#include "write_handler.h"
#include <set>
#include "spsc_queue.h"

namespace std {
    class thread;
}

struct IOEvent;


class IOThread
{
public:
    IOThread();
    virtual ~IOThread();

    virtual bool Init(uint16_t thread_id, const ReadHandler::CreateNetPacketFunc &create_packet_func, 
        const OutputIOEventPipe &output_event_pipe);

    void Start();
    void Stop();
    void Join();
    virtual void Update();

    void AcceptIOEvent(IOEvent *event);

    inline uint16_t GetThreadId() { return m_thread_id; }

protected:
    virtual void HandleEpollEvent(const epoll_event &ev);

    uint32_t HandleIOEvent();
    void OnRegisterConnection(const RegisterConnectionEvent &event);
    void OnWrite(const WriteEvent &event);
    void OnCloseConnectionRequest(const CloseConnectionRequestEvent &event);

    void BeforeOutputIOEvent(IOEvent *io_event);
    void CloseConnection(int32_t connection_fd);

protected:
    uint16_t m_thread_id;
    std::thread *m_thread = nullptr;

    EpollEventManager m_epoll_event_manager;
    OutputIOEventPipe m_output_io_event_pipe;

    ReadHandler m_read_handler;
    WriteHandler m_write_handler;

    SPSCQueue<IOEvent> m_io_events;

    bool m_keep_alive = true;
    timespec m_sleep_duration;
};