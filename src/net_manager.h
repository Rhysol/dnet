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

    int32_t ConnectTo(const std::string &remote_ip, uint16_t remote_port);
    bool Send(int32_t connection_fd, const char *data_bytes, uint32_t data_len);
    void CloseConnection(int32_t connection_fd);
    NetConfig *GetConfig() { return m_net_config; }
private:
    void InitThreads();

    uint16_t HashToIoThread(int32_t connection_fd);
    virtual void Pass2MainThread(IOEvent *event) override;
    void Pass2IOThread(IOEvent *event, uint16_t io_thread_id);

    void OnAcceptConnection(const AcceptConnectionEvent &event);
    void OnRead(const ReadEvent &event);
    void OnCloseConnectionComplete(const CloseConnectionCompleteEvent &event);

private:
    NetConfig *m_net_config = nullptr;
    ListenerThread *m_listener_thread = nullptr;
    std::vector<IOThread *> m_io_threads;

    MPSCQueue<IOEvent> m_events_queue;

    NetEventInterface *m_net_event_handler = nullptr;

    bool m_keep_alive = true;
};

}