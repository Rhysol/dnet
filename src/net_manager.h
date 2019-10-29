#pragma once
#include <vector>
#include "connection_manager.h"
#include "mpsc_queue.h"
#include "io_event.h"


class IOThread;
class ListenerThread;


class NetManager
{
public:
    NetManager();
    ~NetManager();

    bool Init(uint16_t thread_num, const std::string &listen_ip, uint16_t listen_port, NetEventInterface *net_event_handler);
    uint32_t Update();
    void Stop();
    inline bool IsAlive() { return m_keep_alive; }

    const Connection *ConnectTo(const std::string &remote_ip, uint16_t remote_port);
    bool Send(int32_t connection_fd, const char *data_bytes, uint32_t data_len);

private:
    void InitThreads(const std::string &listen_ip, uint16_t listen_port);

    uint16_t HashToIoThread(int32_t connection_fd);
    void AcceptIOEvent(IOEvent *event, uint16_t thread_id);

    void OnAcceptConnection(const AcceptConnectionEvent &event);
    void OnRead(const ReadEvent &event);
    void OnCloseConnectionComplete(const CloseConnectionCompleteEvent &event);

private:
    ListenerThread *m_listener_thread;
    std::vector<IOThread *> m_io_threads;
    uint16_t m_io_threads_num;

    MPSCQueue<IOEvent> m_events_queue;

    ConnectionManager m_connection_manager;

    NetEventInterface *m_net_event_handler;

    bool m_keep_alive = true;
};