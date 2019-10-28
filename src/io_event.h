#pragma once
#include "net_interface.h"

struct IOEvent
{
    enum EventType 
    {
        ACCEPTED_CONNECTION,
        DISCONNECTED,
        READ,
    };
    virtual ~IOEvent() {}
    EventType event_type;
    int32_t connection_fd = -1;
};

struct AcceptedConnectionEvent : public IOEvent
{
    AcceptedConnectionEvent() {
        event_type = ACCEPTED_CONNECTION;
    } 
    std::string remote_ip = "";
    uint16_t remote_port = 0;
};

struct DisconnectEvent : public IOEvent
{
    DisconnectEvent() {
        event_type = DISCONNECTED;
    }
};

struct ReadEvent : public IOEvent
{
    ReadEvent() {
        event_type = READ;
    }
    ~ReadEvent() {
        if (packet != nullptr)
        {
            delete packet;
        }
    }
    NetPacketInterface *packet = nullptr;
}; 