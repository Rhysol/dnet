#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

//一个tcp物理链接
struct Connection
{
    int32_t fd;
    std::string remote_ip;
    uint16_t remote_port;
};

class ConnectionManager
{
public:

    void CloseAllConnections();

    const Connection *ConnectTo(const std::string &remote_ip, uint16_t port);
    const Connection *AddConnection(int32_t connection_fd, const std::string &remote_ip, uint16_t remote_port);

    //主动断开连接接口
    void DisconnectFrom(int32_t fd);

    //未知原因，意外断开
    void AccidentDisconnect(int32_t fd);

private:
    void HandleDuplicatedFd(int32_t fd);

private:
    std::unordered_map<int32_t, Connection> m_connections;
};