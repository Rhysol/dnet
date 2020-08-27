#pragma once
#include "net_interface.h"

namespace dnet
{

struct IOEvent
{
    enum EventType 
    {
        //跨线程事件，io线程和主线程之间
        ACCEPT_CONNECTION, //listener线程接收了新链接，通知到主线程
        REGISTER_CONNECTION, //主线程把新的链接注册到io线程，由io线程进行io操作
        CLOSE_CONNECTION_REQUEST, //主线程向io线程请求关闭链接
        CLOSE_CONNECTION_COMPLETE, //io线程完成关闭链接，通知到主线程
        READ,   //io线程读取了一个完整的网络包，发送给主线程
        WRITE,  //主线程把要发送给指定链接的网络包传给io线程，由io线程进行发送

        //io线程内事件，非跨线程
        UNEXPECTED_DISCONNECT, //读或写的过程中发现链接意外断开
        WRITE_EAGAIN, //写的过程中出现EAGAIN错误
    };
    virtual ~IOEvent() {}
    EventType event_type;
    int32_t connection_fd = -1;
};

struct AcceptConnectionEvent : public IOEvent
{
    AcceptConnectionEvent() {
        event_type = ACCEPT_CONNECTION;
    } 
    std::string remote_ip = "";
    uint16_t remote_port = 0;
};

struct RegisterConnectionEvent : public IOEvent
{
    RegisterConnectionEvent() {
        event_type = REGISTER_CONNECTION;
    }
};

struct CloseConnectionRequestEvent : public IOEvent
{
    CloseConnectionRequestEvent() {
        event_type = CLOSE_CONNECTION_REQUEST;
    }
};

struct CloseConnectionCompleteEvent : public IOEvent
{
    CloseConnectionCompleteEvent() {
        event_type = CLOSE_CONNECTION_COMPLETE;
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
    ReadEvent(const ReadEvent &to_copy) = delete;
    ReadEvent &operator=(const ReadEvent &to_copy) = delete;
    NetPacketInterface *packet = nullptr;
}; 

struct PacketToSend
{
    PacketToSend(int32_t remote_fd, uint32_t len)
    {
        connection_fd = remote_fd;
        packet_bytes = new char[len];
        packet_len = len;
    }
    ~PacketToSend() 
    {
        if (packet_bytes != nullptr)
        {
            delete[] packet_bytes;
        }
    }
    PacketToSend(const PacketToSend &to_copy) = delete;
    PacketToSend &operator=(const PacketToSend &to_copy) = delete;
    PacketToSend(PacketToSend &&to_move)
    {
        this->operator=(std::move(to_move));
    }
    PacketToSend &operator=(PacketToSend &&to_move)
    {
        connection_fd = to_move.connection_fd;
        packet_bytes = to_move.packet_bytes;
        packet_len = to_move.packet_len;
        packet_offset = to_move.packet_offset;

        to_move.connection_fd = -1;
        to_move.packet_bytes = nullptr;
        to_move.packet_len = 0;
        to_move.packet_offset = 0;
        return *this;
    }
    int32_t connection_fd;
    char *packet_bytes;
    uint32_t packet_len;
    uint32_t packet_offset = 0;
};

struct WriteEvent : public IOEvent
{
    WriteEvent() {
        event_type = WRITE;
    }
    ~WriteEvent() {
        if (packet != nullptr)
        {
            delete packet;
        }
    }
    PacketToSend *packet = nullptr;
};

struct UnexpectedDisconnectEvent : public IOEvent
{
    UnexpectedDisconnectEvent() {
        event_type = UNEXPECTED_DISCONNECT;
    }
};

struct WriteEagainEvent : public IOEvent
{
    WriteEagainEvent () {
        event_type = WRITE_EAGAIN;
    }
};

typedef std::function<void (IOEvent *event)> OutputIOEventPipe;

}