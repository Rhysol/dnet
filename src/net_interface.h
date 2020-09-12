#pragma once
#include <functional>
#include <cstring>
#include <vector>

namespace dnet
{

//net_manager通知
class NetEventHandler
{
public:
    virtual void OnAcceptConnection(uint64_t connection_id, const std::string &ip, uint16_t port) = 0;
    virtual uint32_t GetBodyLenFromHeader(const char *header_bytes) = 0;
    virtual void AsyncConnectResult(uint64_t /*connection_id*/, bool /*result*/) {};
    virtual void OnReceivePacket(uint64_t connection_id, std::vector<char> &packet_bytes, uint32_t thread_id) = 0;
    virtual void OnDisconnect(uint64_t connection_id) = 0;
};

}
