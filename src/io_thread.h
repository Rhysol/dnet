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

    virtual bool Init(uint16_t thread_id, const NetConfig *net_config);

    virtual void Pass2MainThread(io_event::IOEvent *io_event) override;
    virtual void Pass2IOThread(io_event::IOEvent *io_event) override;

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
    void HandleEPOLLERR(int32_t fd, Connection &connection);
    void HandleAsyncConnectResult(Connection &connection, bool is_success);

    uint32_t HandleIOEvent();
    void OnRegisterConnection(const io_event::RegisterConnection &event);
    void OnSendAPacket(io_event::SendAPacket&event);

    void RemoveConnection(uint64_t connection_id);
    void RemoveAllConnection();

protected:
    uint16_t m_thread_id;
    std::thread *m_thread = nullptr;
    std::atomic<bool> m_keep_alive;
    timespec m_sleep_duration;

    const NetConfig *m_net_config;
    Connection::CreateNetPacketFunc m_create_packet_func;

	int m_epoll_fd;
	epoll_event *m_epoll_events = nullptr;
    std::unordered_map<uint64_t, Connection> m_id_2_connection;
    std::unordered_map<uint32_t, Connection*> m_fd_2_connection;
    std::vector<uint64_t> m_connections_to_close;

    SPSCQueue<io_event::IOEvent> m_io_events;
};

}