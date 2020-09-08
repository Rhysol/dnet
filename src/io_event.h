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
        UNEXPECTED_DISCONNECT, //读或写的过程中发现链接意外断开
        WRITE_EAGAIN, //写的过程中出现EAGAIN
    };
    virtual ~IOEvent() {}
    EventType event_type;
    uint16_t source_thread_id = -1;
    uint64_t connection_id = 0;
};

struct AcceptConnectionEvent : public IOEvent
{
    AcceptConnectionEvent() {
        event_type = ACCEPT_CONNECTION;
    } 
    std::string remote_ip = "";
    uint16_t remote_port = 0;
    int32_t connection_fd = -1;
};

struct RegisterConnectionEvent : public IOEvent
{
    RegisterConnectionEvent() {
        event_type = REGISTER_CONNECTION;
    }
    int32_t connection_fd = -1;
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
    NetPacketInterface *packet = nullptr;
}; 

struct PacketToSend
{
    PacketToSend(uint32_t len)
    {
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
        packet_bytes = to_move.packet_bytes;
        packet_len = to_move.packet_len;
        packet_offset = to_move.packet_offset;

        to_move.packet_bytes = nullptr;
        to_move.packet_len = 0;
        to_move.packet_offset = 0;
        return *this;
    }
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

class IOEventPasser
{
public:
    enum class EDestination : uint16_t
    {
        MAIN_THREAD, //事件目的地主线程，从io线程到主线程
        IO_THREAD, //事件目的地io线程，从主线程到io线程
        _END
    };

    void SetNextPasser(IOEventPasser *next_passer, EDestination destination)
    {
        if (!next_passer) return;
        if (destination == EDestination::MAIN_THREAD)
        {
            m_next_passer_2_main_thread = next_passer;
        }
        else if (destination == EDestination::IO_THREAD)
        {
            m_next_passer_2_io_thread = next_passer;
        }
    }

    virtual void Pass2IOThread(IOEvent *event)
    {
        if (!event || !m_next_passer_2_io_thread) return;
        m_next_passer_2_io_thread->Pass2IOThread(event);
    }

    virtual void Pass2MainThread(IOEvent *event)
    {
        if (!event || !m_next_passer_2_main_thread) return;
        m_next_passer_2_main_thread->Pass2MainThread(event);
    }

protected:
    IOEventPasser *m_next_passer_2_io_thread = nullptr;
    IOEventPasser *m_next_passer_2_main_thread = nullptr;
};


}