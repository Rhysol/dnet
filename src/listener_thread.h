#pragma once
#include "io_thread.h"
#include "listener.h"

namespace dnet
{

class NetConfig;

class ListenerThread : public IOThread
{
public:
    ListenerThread();
    ~ListenerThread();

    bool Init(uint16_t thread_id, const NetConfig *net_config,
        const ReadHandler::CreateNetPacketFunc &create_packet_func, 
        const OutputIOEventPipe &output_event_pipe);

private:
    virtual void HandleEpollEvent(const epoll_event &ev) override;

private:
    Listener m_listener;
};

}