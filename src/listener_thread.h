#pragma once
#include "io_thread.h"
#include "listener.h"


class ListenerThread : public IOThread
{
public:
    ListenerThread();
    ~ListenerThread();

    bool Init(uint16_t thread_id, const std::string &listen_ip, uint16_t listen_port,
        const CreateNetPacketFunc &create_packet_func, 
        const EpollEventHandler::OutputIOEventPipe &output_event_pipe);

private:
    Listener m_listener;
};