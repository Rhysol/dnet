#pragma once
#include <cstdint>
#include <string>
#include <functional>
#include <vector>
#include <unordered_map>
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

struct ThreadReadBuffer
{
    inline static uint32_t BufferMaxLen() {
        static uint32_t buffer_max_len = 65535;
        return buffer_max_len;
    }
    char buffer[65535];
    uint32_t buffer_len = 0;
};

class IOEventPipe
{
public:
    IOEventPipe();
    ~IOEventPipe();

    typedef std::function<void (IOEvent *event, uint16_t thread_id)> AcceptEventFunc;
    typedef std::function<NetPacketInterface *()> CreateNetPacketFunc;
    bool Init(uint16_t thread_num, const AcceptEventFunc &accept_event_func, const CreateNetPacketFunc &create_packet_func);

    //在listener_thread中被调用
    void OnAccept(int32_t listener_fd, uint16_t thread_id);
    //在io_thread中被调用
    void OnRead(int32_t connection_fd, uint16_t thread_id);

private:
    void ParseReadBuffer(int32_t connection_fd, uint16_t thread_id);
    ReadEvent *GetUnfinishedReadEvent(int32_t connection_fd, uint16_t thread_id);
    void OnDisconnect(int32_t connection_fd, uint16_t thread_id); 
    ReadEvent *CreateReadEvent(int32_t connection_fd, uint16_t thread_id);

private:
    uint16_t m_thread_num;
    AcceptEventFunc m_accept_event_func;
    CreateNetPacketFunc m_create_net_packet_func;

    ThreadReadBuffer *m_thread_read_buffer;
    std::unordered_map<int32_t, ReadEvent *> *m_unfinished_read = nullptr;
};