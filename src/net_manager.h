#pragma once
#include <vector>
#include "connection_manager.h"
#include "io_event_pipe.h"
#include "mpsc_queue.h"
#include "net_packet_interface.h"


class IOThread;
class ListenerThread;


class NetManager
{
public:
    NetManager();
    ~NetManager();

    bool Init(uint16_t thread_num, const std::string &listen_ip, uint16_t listen_port,
        const CreateNetPacketFunc &create_packet_func, const HandlePacketFunc &handle_packet_func);
    uint32_t Update();
    void Stop();
    inline bool IsAlive() { return m_keep_alive; }

    const Connection *ConnectTo(const std::string &remote_ip, uint16_t remote_port);

private:
    void InitThreads(const std::string &listen_ip, uint16_t listen_port);

    uint16_t HashToIoThread(int32_t connection_fd);
    void AcceptIOEvent(IOEvent *event, uint16_t thread_id);
    void HandleAcceptedConnectionEvent(const AcceptedConnectionEvent &event);
    void HandleReadEvent(const ReadEvent &event);

private:
    ListenerThread *m_listener_thread;
    std::vector<IOThread *> m_io_threads;
    uint16_t m_io_threads_num;

    IOEventPipe m_io_event_pipe;
    MPSCQueue<IOEvent> m_events_queue;

    HandlePacketFunc m_handle_packet_func;

    ConnectionManager m_connection_manager;

    bool m_keep_alive = true;
};