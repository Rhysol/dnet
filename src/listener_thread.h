#pragma once
#include "io_thread.h"

namespace dnet
{

class NetConfig;

class ListenerThread : public IOThread
{
public:
    ListenerThread();
    ~ListenerThread();

    virtual bool Init(uint16_t thread_id, const NetConfig *net_config) override;
    bool StartListen();
private:
    virtual void HandleEpollEvent(const epoll_event &ev) override;
    void OnAccept();

private:
	int32_t m_listener_fd = -1;
};

}