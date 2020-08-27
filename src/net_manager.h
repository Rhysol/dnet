#pragma once
#include <vector>
#include "connection_manager.h"
#include "mpsc_queue.h"
#include "io_event.h"
#include "net_config.h"

namespace dnet
{

class IOThread;
class ListenerThread;


class NetManager
{
public:
    NetManager();
    ~NetManager();

    bool Init(const NetConfig &config, NetEventInterface *net_event_handler);
    uint32_t Update();
    void Stop();
    inline bool IsAlive() { return m_keep_alive; }

    const Connection *ConnectTo(const std::string &remote_ip, uint16_t remote_port);
    bool Send(int32_t connection_fd, const char *data_bytes, uint32_t data_len);
    void CloseConnection(int32_t connection_fd);

private:
    void InitLogger();
    void InitThreads();

    uint16_t HashToIoThread(int32_t connection_fd);
    void AcceptIOEvent(IOEvent *event, uint16_t thread_id);

    void OnAcceptConnection(const AcceptConnectionEvent &event);
    void OnRead(const ReadEvent &event);
    void OnCloseConnectionComplete(const CloseConnectionCompleteEvent &event);

    void CloseAllConnections();

private:
    NetConfig m_net_config;
    ListenerThread *m_listener_thread;
    std::vector<IOThread *> m_io_threads;

    MPSCQueue<IOEvent> m_events_queue;

    ConnectionManager m_connection_manager;

    NetEventInterface *m_net_event_handler;

    bool m_keep_alive = true;
};

}