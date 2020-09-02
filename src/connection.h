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
    ReadBuffer(const ReadBuffer &to_copy)
    {
        this->operator=(to_copy);
    }
    ReadBuffer &operator=(const ReadBuffer &to_copy)
    {
        buffer = new char[65535];
        memcpy(buffer, to_copy.buffer, to_copy.buffer_len);
        buffer_len = to_copy.buffer_len;
        return *this;
    }
    ReadBuffer(ReadBuffer &&to_move)
    {
        this->operator=(std::move(to_move));
    }
    ReadBuffer &operator=(ReadBuffer &&to_move)
    {
        buffer = to_move.buffer;
        to_move.buffer = nullptr;
        buffer_len = to_move.buffer_len;
        to_move.buffer_len = 0;
        return *this;
    }
    inline static constexpr uint32_t BufferMaxLen() {
        return 65535;
    }
    char *buffer;
    int32_t buffer_len = 0;
};

class Connection : public IOEventPasser
{
public:
    Connection();
    ~Connection();

    typedef std::function<NetPacketInterface *()> CreateNetPacketFunc;
    void Init(int32_t fd, const NetConfig *net_config, CreateNetPacketFunc create_net_packet_func);
    inline bool HasInited() { return m_has_inited; }

    void Receive();

    bool Send(PacketToSend *packet);
    //发送完所有积压的包返回true, 否则返回false
    bool SendRemainPacket();
    int32_t GetConnectionFD() { return m_fd; }

private:
    void ParseReadBuffer();
    ReadEvent *CreateReadEvent();

    void OnWriteEagain();

    //读或者写的过程中发现连接断开，服务器被动断开连接
    void OnUnexpectedDisconnect();
private:
    int32_t m_fd;
    bool m_has_inited = false;
    bool m_to_close = false;
    const NetConfig *m_net_config = nullptr;
    CreateNetPacketFunc m_create_net_packet_func;

    ReadBuffer m_read_buffer;
    ReadEvent *m_unfinished_read = nullptr;

    std::deque<PacketToSend *> m_packet_to_send;
};

}