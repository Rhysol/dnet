#pragma once
#include <sys/epoll.h>
#include <time.h>
#include <set>
#include "spsc_queue.h"
#include "io_event.h"
#include <unordered_map>
#include <unordered_set>
#include "connection.h"
#include <vector>

namespace std {
    class thread;
}

namespace dnet
{

struct IOEvent;
struct NetConfig;

class IOThread : public IOEventPasser
{
public:
    IOThread();
    virtual ~IOThread();

    virtual bool Init(uint16_t thread_id, const Connection::CreateNetPacketFunc &create_packet_func, const NetConfig *net_config);

    virtual void Pass2MainThread(IOEvent *io_event) override;
    virtual void Pass2IOThread(IOEvent *io_event) override;

protected:
    virtual void HandleEpollEvent(const epoll_event &ev);

public:
    void Start();
    void Stop();
    void Join();
    void Update();
    inline uint16_t GetThreadId() { return m_thread_id; }

protected:
    bool EpollCtl(int32_t fd, uint32_t events, int32_t operation);

    uint32_t HandleIOEvent();
    void OnRegisterConnection(const RegisterConnectionEvent &event);
    void OnWrite(WriteEvent &event);
    void OnCloseConnectionRequest(const CloseConnectionRequestEvent &event);

    void CloseConnection(int32_t connection_fd);
    void CloseAllConnection();

protected:
    uint16_t m_thread_id;
    std::thread *m_thread = nullptr;
    std::atomic<bool> m_keep_alive;
    timespec m_sleep_duration;

    const NetConfig *m_net_config;
    Connection::CreateNetPacketFunc m_create_packet_func;

	int m_epoll_fd;
	epoll_event *m_epoll_events = nullptr;
    std::unordered_map<int32_t, Connection> m_connections;
    std::vector<int32_t> m_connections_to_close;

    SPSCQueue<IOEvent> m_io_events;
};

}