#pragma once
#include <vector>
#include "mpsc_queue.h"
#include "io_event.h"
#include "net_config.h"

struct sockaddr_in;

namespace dnet
{

class IOThread;
class ListenerThread;

class NetManager : public IOEventPasser
{
public:
    NetManager();
    ~NetManager();

    bool Init(const NetConfig &config, NetEventInterface *net_event_handler);
    uint32_t Update();
    void Stop();
    inline bool IsAlive() { return m_keep_alive; }

    //异步连接的结果在NetEventInterface::AsyncConnectResult中通知
    uint64_t ConnectTo(const std::string &remote_ip, uint16_t remote_port, bool async = false);
    bool Send(uint64_t connection_id, const char *data_bytes, uint32_t data_len);
    void CloseConnection(uint64_t connection_id);
    NetConfig *GetConfig() { return m_net_config; }
private:
    bool InitThreads();

    bool BlockingConnect(int32_t connection_fd, sockaddr_in &remote_addr, const std::string &remote_ip, uint16_t remote_port);
    bool NonBlockingConnect(int32_t connection_fd, sockaddr_in &remote_addr, const std::string &remote_ip, uint16_t remote_port);

    uint16_t HashToIoThread(uint64_t connection_id);
    virtual void Pass2MainThread(io_event::IOEvent *event) override;
    virtual void Pass2IOThread(io_event::IOEvent *event) override;

    void OnAcceptConnection(const io_event::AcceptConnection &event);
    inline uint64_t AllocateConnectionId() { return m_next_connection_id++; }
    void OnReceiveAPacket(const io_event::ReceiveAPacket &event);
    void OnDisconnect(uint64_t connection_id);
    void OnAsyncConnectResult(const io_event::AsyncConnectResult &event);
private:
    NetConfig *m_net_config = nullptr;
    ListenerThread *m_listener_thread = nullptr;
    std::vector<IOThread *> m_io_threads;
    uint64_t m_next_connection_id = 1;

    MPSCQueue<io_event::IOEvent> m_events_queue;

    NetEventInterface *m_net_event_handler = nullptr;

    bool m_keep_alive = true;
};

}