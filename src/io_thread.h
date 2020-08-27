#pragma once
#include <sys/epoll.h>
#include <time.h>
#include "read_handler.h"
#include "write_handler.h"
#include <set>
#include "spsc_queue.h"

namespace std {
    class thread;
}

namespace dnet
{

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

	//使epoll监听fd的事件，如果fd已经被监听，则更新监听事件
	bool MonitorFd(int32_t fd, epoll_event &event);
	//将fd从epoll的监听列表中移除
	void StopMonitorFd(int32_t fd);
	bool IsFdMonitored(int32_t fd);

protected:
    uint16_t m_thread_id;
    std::thread *m_thread = nullptr;

	int m_epoll_fd;
	epoll_event *m_events = nullptr;
	std::set<int32_t> m_monitoring_fd;

    OutputIOEventPipe m_output_io_event_pipe;

    ReadHandler m_read_handler;
    WriteHandler m_write_handler;

    SPSCQueue<IOEvent> m_io_events;

    std::atomic<bool> m_keep_alive;
    timespec m_sleep_duration;
};

}