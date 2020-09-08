#pragma once
#include <vector>
#include "mpsc_queue.h"
#include "io_event.h"
#include "net_config.h"

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

    uint64_t ConnectTo(const std::string &remote_ip, uint16_t remote_port);
    bool Send(uint64_t connection_id, const char *data_bytes, uint32_t data_len);
    void CloseConnection(uint64_t connection_id);
    NetConfig *GetConfig() { return m_net_config; }
private:
    void InitThreads();

    uint16_t HashToIoThread(uint64_t connection_id);
    virtual void Pass2MainThread(IOEvent *event) override;
    virtual void Pass2IOThread(IOEvent *event) override;

    void OnAcceptConnection(const AcceptConnectionEvent &event);
    inline uint64_t AllocateConnectionId() { return m_next_connection_id++; }
    void OnRead(const ReadEvent &event);
    void OnCloseConnectionComplete(const CloseConnectionCompleteEvent &event);

private:
    NetConfig *m_net_config = nullptr;
    ListenerThread *m_listener_thread = nullptr;
    std::vector<IOThread *> m_io_threads;
    uint64_t m_next_connection_id = 1;

    MPSCQueue<IOEvent> m_events_queue;

    NetEventInterface *m_net_event_handler = nullptr;

    bool m_keep_alive = true;
};

}