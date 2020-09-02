#pragma once
#include <functional>

namespace dnet
{

class NetPacketInterface
{
public:
    NetPacketInterface(uint32_t header_len) : header_len(header_len) {
        header = new char[header_len];
    }
    virtual ~NetPacketInterface() {
        delete[] header;
        if (body != nullptr)
        {
            delete[] body;
        }
    }
    //从header中获取包体的长度
    virtual uint32_t ParseBodyLenFromHeader() = 0;

    char *header = nullptr;
    uint32_t header_len = 0;
    uint32_t header_offset = 0;
    char *body = nullptr;
    uint32_t body_len = 0;
    uint32_t body_offset = 0;
};


//net_manager通知
class NetEventInterface
{
public:
    //告诉net_manager如何创建NetPacket，使用者自定义
    virtual NetPacketInterface *CreateNetPacket() = 0;
    virtual void OnNewConnection(int32_t connection_fd, const std::string &ip, uint16_t port) = 0;
    virtual void OnReceivePacket(int32_t connection_fd, const NetPacketInterface &) = 0;
    virtual void OnDisconnect(int32_t connection_fd) = 0;
};

}
