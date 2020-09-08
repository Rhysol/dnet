#include "connection.h"
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include "logger.h"

using namespace dnet;

thread_local ReadBuffer Connection::m_read_buffer;

Connection::Connection()
{

}

Connection::~Connection()
{
    if (m_unfinished_read)
    {
        delete m_unfinished_read;
    }
    for (PacketToSend *packet : m_packet_to_send)
    {
        delete packet;
    }
}

void Connection::Init(uint64_t id, int32_t fd, const NetConfig *net_config)
{
    m_id = id;
    m_fd = fd;
    m_net_config = net_config;
    m_has_inited = true;
}

void Connection::Receive()
{
    int32_t read_len = -1;
    do {
        read_len = recv(m_fd, m_read_buffer.buffer, ReadBuffer::Capacity(), 0);
        if (read_len > 0)
        {
            m_read_buffer.size = read_len;
            ParseReadBuffer();
        }
        else if (-1 == read_len)
        {
            if (EAGAIN != errno)
            {
                OnUnexpectedDisconnect();
            }
            return;
        }
        else if (read_len == 0)
        {
            OnUnexpectedDisconnect();
            return;
        }
    } while (true);
}

void Connection::ParseReadBuffer()
{
    if (!m_unfinished_read)
    {
        m_unfinished_read = CreateReadEvent();
    }
    NetPacketInterface *packet = m_unfinished_read->packet;

    uint32_t len_to_read = 0;
    while (m_read_buffer.offset != m_read_buffer.size)
    {
        UpdatePacketDataCapacity(packet, m_read_buffer.buffer + m_read_buffer.offset, m_read_buffer.size - m_read_buffer.offset);
        len_to_read = std::min<uint32_t>(packet->data_capacity - packet->data_size, m_read_buffer.size - m_read_buffer.offset);
        memcpy(packet->data + packet->data_size, m_read_buffer.buffer, len_to_read);
        packet->data_size += len_to_read;
        m_read_buffer.offset += len_to_read;
        if (packet->data_capacity_adjusted_success &&
            packet->data_size == packet->data_capacity)
        {
            Pass2MainThread(m_unfinished_read);
            m_unfinished_read = CreateReadEvent();
            packet = m_unfinished_read->packet;
        }
    }
    m_read_buffer.offset = 0;
}

void Connection::UpdatePacketDataCapacity(NetPacketInterface *packet, const char *bytes, uint32_t bytes_len)
{
    if (packet->data == nullptr)
    {
        if (bytes_len < packet->header_len)
        {
            packet->data_capacity = packet->header_len;
        }
        else
        {
            packet->data_capacity = packet->header_len + packet->ParseBodyLenFromHeader(bytes);
            packet->data_capacity_adjusted_success = true;
        }
        packet->data = new char[packet->data_capacity];
    }
    else
    {
        if (!packet->data_capacity_adjusted_success &&
            packet->data_size + bytes_len >= packet->header_len)
        {
            memcpy(packet->data + packet->data_size, bytes, packet->header_len - packet->data_size);
            uint32_t body_len = packet->ParseBodyLenFromHeader(packet->data);
            char *temp = new char[packet->header_len + body_len];
            memcpy(temp, packet->data, packet->data_size);
            delete[] packet->data;
            packet->data = temp;
            packet->data_capacity = packet->header_len + body_len;
            packet->data_capacity_adjusted_success = true;
        }
    }
}

ReadEvent *Connection::CreateReadEvent()
{
    ReadEvent *event = new ReadEvent();
    event->packet = m_net_config->create_net_packet_func();
    event->connection_id = m_id;
    return event;
}

void Connection::OnUnexpectedDisconnect()
{
    if (m_to_close) return;
    m_to_close = true;
    UnexpectedDisconnectEvent *event = new UnexpectedDisconnectEvent;
    event->connection_id = m_id;
    Pass2MainThread(event);
}

bool Connection::Send(PacketToSend *packet)
{
    m_packet_to_send.push_back(new PacketToSend(std::move(*packet)));
    if (m_can_send)
    {
        DoSendRemainPacket();
    }
    //积压超过一定数量就断开链接
    if (m_packet_to_send.size() > m_net_config->max_unfinished_send_packet)
    {
        LOGW("fd: {} unsended packet more than {}, close connection!", m_fd, m_net_config->max_unfinished_send_packet);
        OnUnexpectedDisconnect();
    }
    return m_packet_to_send.empty();
}

bool Connection::SendRemainPacket()
{
    m_can_send = true;
    return DoSendRemainPacket();
}

bool Connection::DoSendRemainPacket()
{
    PacketToSend *packet = nullptr;
    const char *bytes_to_write = 0;
    int32_t len_to_write = 0;
    int32_t write_len = 0;
    while (m_packet_to_send.size() != 0)
    {
        packet = m_packet_to_send.front();
        bytes_to_write = packet->packet_bytes + packet->packet_offset;
        len_to_write = packet->packet_len - packet->packet_offset;
        write_len = send(m_fd, bytes_to_write, len_to_write, MSG_NOSIGNAL);
        if (write_len == -1)
        {
            if (EAGAIN == errno)
            {
                OnWriteEagain();
            }
            else
            {
                OnUnexpectedDisconnect();
            }
            m_can_send = false;
            break;
        }
        else if (len_to_write == write_len)
        {
            m_packet_to_send.pop_front();
            delete packet;
        }
        else
        {
            packet->packet_offset += write_len;
        }
    }
    return m_packet_to_send.empty();
}

void Connection::OnWriteEagain()
{
    WriteEagainEvent *event = new WriteEagainEvent;
    event->connection_id = m_id;
    Pass2MainThread(event);
}
