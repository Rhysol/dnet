#include "write_handler.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include "net_config.h"

WriteHandler::WriteHandler()
{

}

WriteHandler::~WriteHandler()
{

}

void WriteHandler::Init(const OutputIOEventPipe &pipe)
{
    m_output_io_event_pipe = pipe;
}

bool WriteHandler::HandleUnfinishedPacket(int32_t connection_fd)
{
    auto unfinished_queue_iter = m_unfinished_packet.find(connection_fd);
    if (unfinished_queue_iter == m_unfinished_packet.end()) return true;
    std::deque<PacketToSend *> &unfinished_queue = unfinished_queue_iter->second;
    SendUnfinishedPacket(unfinished_queue);
    if (unfinished_queue.size() == 0)
    {
        m_unfinished_packet.erase(connection_fd);
        return true;
    }
    return false;
}

void WriteHandler::SendUnfinishedPacket(std::deque<PacketToSend *> &unfinished_queue)
{
    PacketToSend *packet = nullptr;
    const char *bytes_to_write = 0;
    int32_t len_to_write = 0;
    int32_t write_len = 0;
    while (unfinished_queue.size() != 0)
    {
        packet = unfinished_queue.front();
        bytes_to_write = packet->packet_bytes + packet->packet_offset;
        len_to_write = packet->packet_len - packet->packet_offset;
        write_len = write(packet->connection_fd, bytes_to_write, len_to_write);
        if (write_len == -1 && errno != EAGAIN)
        {
            OnUnexpectedDisconnect(packet->connection_fd);
            break;
        }
        else if (len_to_write == write_len)
        {
            unfinished_queue.pop_front();
            delete packet;
        }
        else
        {
            packet->packet_offset += write_len;
            break;
        }
    }
}

void WriteHandler::Send(PacketToSend *packet)
{
    const char *bytes_to_write = packet->packet_bytes + packet->packet_offset;
    int32_t len_to_write = packet->packet_len - packet->packet_offset;
    int32_t write_len = write(packet->connection_fd, bytes_to_write, len_to_write);
    if (write_len > 0)
    {
        packet->packet_offset += write_len;
        if (write_len < len_to_write)
        {
            OnUnfinishedPacket(packet);
        }
    }
    else if (write_len == -1)
    {
        if (errno == EAGAIN)
        {
            OnUnfinishedPacket(packet);
            OnWriteEagain(packet->connection_fd);
        }
        else
        {
            OnUnexpectedDisconnect(packet->connection_fd);
        }
    }
}

void WriteHandler::OnUnfinishedPacket(PacketToSend *packet)
{
    std::deque<PacketToSend *> &unfinished_packet = m_unfinished_packet[packet->connection_fd];
    unfinished_packet.push_back(new PacketToSend(std::move(*packet)));
    //积压超过一定数量就断开链接
    if (unfinished_packet.size() > global_config.max_unfinished_send_packet)
    {
        std::cout << "fd:" << packet->connection_fd << " unsended packet more than 10, close connection!" << std::endl;
        OnUnexpectedDisconnect(packet->connection_fd);
    }
}

void WriteHandler::ClearUnfinishedPacket(int32_t connection_fd)
{
    auto iter = m_unfinished_packet.find(connection_fd);
    if (iter != m_unfinished_packet.end())
    {
        for (PacketToSend *packet : iter->second)
        {
            delete packet;
        }
        m_unfinished_packet.erase(connection_fd);
    }
}

void WriteHandler::OnCloseConnection(int32_t connection_fd)
{
    ClearUnfinishedPacket(connection_fd);
}

void WriteHandler::OnUnexpectedDisconnect(int32_t connection_fd)
{
    UnexpectedDisconnectEvent *event = new UnexpectedDisconnectEvent;
    event->connection_fd = connection_fd;
    m_output_io_event_pipe(event);
}

void WriteHandler::OnWriteEagain(int32_t connection_fd)
{
    WriteEagainEvent *event = new WriteEagainEvent;
    event->connection_fd = connection_fd;
    m_output_io_event_pipe(event);
}