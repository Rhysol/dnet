#pragma once
#include "io_event.h"
#include <unordered_map>
#include <deque>
#include "net_config.h"
#include <cstring>

namespace dnet
{

struct ReadBuffer
{
    ReadBuffer() {
        buffer = new char[65535];
    }
    ~ReadBuffer() {
        if (buffer)
        {
            delete[] buffer;
        }
    }
    inline static constexpr uint32_t Capacity() {
        return 65535;
    }
    char *buffer;
    int32_t size = 0;
    int32_t offset = 0;
};

class Connection : public IOEventPasser
{
public:
    Connection();
    ~Connection();

    typedef std::function<NetPacketInterface *()> CreateNetPacketFunc;
    void Init(uint64_t id, int32_t fd, const NetConfig *net_config);
    inline bool HasInited() { return m_has_inited; }

    void Receive();

    bool Send(PacketToSend *packet);
    //发送完所有积压的包返回true, 否则返回false
    bool SendRemainPacket();
    inline uint64_t GetID() { return m_id; }
    inline int32_t GetFD() { return m_fd; }

    inline bool Connected() { return m_connected; }
    void SetConnected() { m_connected = true; }

    //读或者写的过程中发现连接断开，服务器被动断开连接
    void OnUnexpectedDisconnect();
private:
    void ParseReadBuffer();
    void UpdatePacketDataCapacity(NetPacketInterface *packet, const char *bytes, uint32_t bytes_len);
    io_event::ReceiveAPacket *CreateReceiveAPacketEvent();

    bool DoSendRemainPacket();

private:
    uint64_t m_id = 0;
    int32_t m_fd = -1;
    bool m_has_inited = false;
    bool m_connected = true;
    bool m_to_close = false;
    const NetConfig *m_net_config = nullptr;

    thread_local static ReadBuffer m_read_buffer;
    io_event::ReceiveAPacket *m_incomplete_receive = nullptr;

    bool m_can_send = true;
    std::deque<PacketToSend *> m_packet_to_send;
};

}