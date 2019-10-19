#pragma once
#include <functional>


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

//告诉底层如何创建NetPacket，由使用者自定义
typedef std::function<NetPacketInterface *()> CreateNetPacketFunc;
//告诉底层如果处理Netpacket, 由使用者自定义
typedef std::function<void (const NetPacketInterface &)> HandlePacketFunc;
