#include "connection_manager.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>
#include "logger.h"

using namespace dnet;

const Connection *ConnectionManager::ConnectTo(const std::string &remote_ip, uint16_t remote_port)
{
    sockaddr_in remote_addr;
    remote_addr.sin_family = AF_INET;
    if (inet_aton(remote_ip.c_str(), &remote_addr.sin_addr) == 0)
    {
        LOGE("ip:{} is invalid!", remote_ip);
        return nullptr;
    }
    remote_addr.sin_port = htons(remote_port);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (connect(fd, (sockaddr *)&remote_addr, sizeof(sockaddr)) == -1)
    {
        LOGE("connect to: {}:{} failed! errno: {}", remote_ip, remote_port, errno);
        return nullptr;
    }

    if (fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK) == -1)
    {
        LOGE("set to nonblock failed! errno: {}", errno);
        return nullptr;
    }

    HandleDuplicatedFd(fd);
    Connection &connection = m_connections[fd];
    connection.fd = fd;
    connection.remote_ip = remote_ip;
    connection.remote_port = remote_port;

    return &connection;
}

void ConnectionManager::HandleDuplicatedFd(int32_t fd)
{
    auto iter = m_connections.find(fd);
    if (iter != m_connections.end())
    {
        LOGW("Duplicated fd: {} in manager", fd);
    }
}

const Connection *ConnectionManager::AddConnection(int32_t connection_fd, const std::string &remote_ip, uint16_t remote_port)
{
    HandleDuplicatedFd(connection_fd);
    Connection &connection = m_connections[connection_fd];
    connection.fd = connection_fd;
    connection.remote_ip = remote_ip;
    connection.remote_port = remote_port;
    return &connection;
}

void ConnectionManager::DisconnectFrom(int32_t fd)
{
    close(fd);
    m_connections.erase(fd);
}

const Connection *ConnectionManager::GetConnection(int32_t connection_fd)
{
    auto iter = m_connections.find(connection_fd);
    if (iter != m_connections.end())
    {
        return &iter->second;
    }
    return nullptr;
}