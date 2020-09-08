#pragma once
#include <functional>
#include <cstring>

namespace dnet
{

class NetPacketInterface
{
public:
    NetPacketInterface(uint32_t header_len) : header_len(header_len){}
    virtual ~NetPacketInterface() {
        if (data)
        {
            delete[] data;
        }
    }

    NetPacketInterface(const NetPacketInterface &to_copy)
    {
        this->operator=(to_copy);
    }
    NetPacketInterface &operator=(const NetPacketInterface &to_copy)
    {
        if (this == &to_copy) return *this;
        char *temp = new char[to_copy.data_capacity];
        memcpy(temp, to_copy.data, to_copy.data_capacity);
        if (data)
        {
            delete[] data;
        }
        data = temp;
        data_size = to_copy.data_size;
        data_capacity = to_copy.data_capacity;
        data_capacity_adjusted_success = to_copy.data_capacity_adjusted_success;
        header_len = to_copy.header_len;
        return *this;
    }
    NetPacketInterface(NetPacketInterface &&to_move)
    {
        this->operator=(std::move(to_move));
    }
    NetPacketInterface &operator=(NetPacketInterface &&to_move)
    {
        if (this == &to_move) return *this;
        data = to_move.data;
        data_size = to_move.data_size;
        data_capacity = to_move.data_capacity;
        data_capacity_adjusted_success = to_move.data_capacity_adjusted_success;
        header_len = to_move.header_len;

        to_move.data = nullptr;
        to_move.data_size = 0;
        to_move.data_capacity = 0;
        to_move.data_capacity_adjusted_success = false;
        return *this;
    }

    //从header中获取包体的长度
    virtual uint32_t ParseBodyLenFromHeader(const char *header_bytes) = 0;

    char *data = nullptr;
    uint32_t data_size = 0;
    uint32_t data_capacity = 0;
    bool data_capacity_adjusted_success = false;

    uint32_t header_len = 0;
};


//net_manager通知
class NetEventInterface
{
public:
    //告诉net_manager如何创建NetPacket，使用者自定义
    virtual NetPacketInterface *CreateNetPacket() = 0;
    virtual void OnNewConnection(uint64_t connection_id, const std::string &ip, uint16_t port) = 0;
    virtual void OnReceivePacket(uint64_t connection_id, NetPacketInterface &packet, uint32_t thread_id) = 0;
    virtual void OnDisconnect(uint64_t connection_id) = 0;
};

}
