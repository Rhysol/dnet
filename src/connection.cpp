#include "connection.h"
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include "logger.h"

using namespace dnet;

Connection::Connection()
{

}

Connection::~Connection()
{
    if (m_unfinished_read)
    {
        delete m_unfinished_read;
    }
    for (PacketToSend *packet : m_unfinished_write)
    {
        delete packet;
    }
}

void Connection::Init(int32_t fd, const NetConfig *net_config, CreateNetPacketFunc create_net_packet_func)
{
    m_fd = fd;
    m_net_config = net_config;
    m_create_net_packet_func = create_net_packet_func;
}

void Connection::Read()
{
    int32_t read_len = -1;
    do {
        read_len = read(m_fd, m_read_buffer.buffer, ReadBuffer::BufferMaxLen());
        if (read_len > 0)
        {
            m_read_buffer.buffer_len = read_len;
            ParseReadBuffer();
        }
        else if (read_len == 0)
        {
            //OnUnexpectedDisconnect(connection_fd);
            return;
        }
        else if (read_len == -1)
        {
            return;
        }
    } while (true);
}

void Connection::ParseReadBuffer()
{
    ReadEvent *event = CreateReadEvent();
    NetPacketInterface *packet = event->packet;

    int32_t len_to_read = 0;
    int32_t read_buffer_offset = 0;
    while (m_read_buffer.buffer_len != 0)
    {
        //读取包头
        if (packet->header_offset < packet->header_len)
        {
            len_to_read = packet->header_len - packet->header_offset;
            if (m_read_buffer.buffer_len < len_to_read)
            {
                memcpy(packet->header + packet->header_offset, m_read_buffer.buffer + read_buffer_offset, m_read_buffer.buffer_len);
                packet->header_offset += m_read_buffer.buffer_len;
                read_buffer_offset += m_read_buffer.buffer_len;
                m_read_buffer.buffer_len = 0;
                m_unfinished_read = event;
            }
            else
            {
                memcpy(packet->header + packet->header_offset, m_read_buffer.buffer + read_buffer_offset, len_to_read);
                packet->header_offset += len_to_read;
                read_buffer_offset += len_to_read;
                m_read_buffer.buffer_len -= len_to_read;
                packet->body_len = packet->ParseBodyLenFromHeader();
                packet->body = new char[packet->body_len];
            }
        }
        else //读取body
        {
            len_to_read = packet->body_len - packet->body_offset;
            if (m_read_buffer.buffer_len < len_to_read)
            {
                memcpy(packet->body + packet->body_offset, m_read_buffer.buffer + read_buffer_offset, m_read_buffer.buffer_len);
                packet->body_offset += m_read_buffer.buffer_len;
                read_buffer_offset += m_read_buffer.buffer_len;
                m_read_buffer.buffer_len = 0;
                m_unfinished_read = event;
            }
            else
            {
                memcpy(packet->body + packet->body_offset, m_read_buffer.buffer + read_buffer_offset, len_to_read);
                packet->body_offset += len_to_read;
                read_buffer_offset += len_to_read;
                m_read_buffer.buffer_len -= len_to_read;
                Pass2MainThread(event);
                event = CreateReadEvent();
                packet = event->packet;
            }
        }
    }
}

ReadEvent *Connection::CreateReadEvent()
{
    ReadEvent *event = nullptr;
    if (!m_unfinished_read)
    {
        event = new ReadEvent;
        event->packet = m_create_net_packet_func();
        event->packet->body_len = event->packet->ParseBodyLenFromHeader();
        event->connection_fd = m_fd;
        return event;
    }
    else
    {
        event = m_unfinished_read;
        m_unfinished_read = nullptr;
    }
    return event;
}

void Connection::OnUnexpectedDisconnect()
{
    UnexpectedDisconnectEvent *event = new UnexpectedDisconnectEvent;
    event->connection_fd = m_fd;
    Pass2MainThread(event);
}

void Connection::Write(PacketToSend *packet)
{
    const char *bytes_to_write = packet->packet_bytes + packet->packet_offset;
    int32_t len_to_write = packet->packet_len - packet->packet_offset;
    int32_t write_len = write(packet->connection_fd, bytes_to_write, len_to_write);
    if (write_len > 0)
    {
        packet->packet_offset += write_len;
        if (write_len < len_to_write)
        {
            OnWriteUnfinished(packet);
        }
    }
    else if (write_len == -1)
    {
        if (errno == EAGAIN)
        {
            OnWriteUnfinished(packet);
            OnWriteEagain();
        }
        else
        {
            OnUnexpectedDisconnect();
        }
    }
}

void Connection::OnWriteUnfinished(PacketToSend *packet)
{
    m_unfinished_write.push_back(new PacketToSend(std::move(*packet)));
    //积压超过一定数量就断开链接
    if (m_unfinished_write.size() > global_config.max_unfinished_send_packet)
    {
        LOGW("fd: {} unsended packet more than 10, close connection!", packet->connection_fd);
        OnUnexpectedDisconnect();
    }
}

bool Connection::HandleUnfinishedWrite()
{
    PacketToSend *packet = nullptr;
    const char *bytes_to_write = 0;
    int32_t len_to_write = 0;
    int32_t write_len = 0;
    while (m_unfinished_write.size() != 0)
    {
        packet = m_unfinished_write.front();
        bytes_to_write = packet->packet_bytes + packet->packet_offset;
        len_to_write = packet->packet_len - packet->packet_offset;
        write_len = write(packet->connection_fd, bytes_to_write, len_to_write);
        if (write_len == -1 && errno != EAGAIN)
        {
            OnUnexpectedDisconnect();
            break;
        }
        else if (len_to_write == write_len)
        {
            m_unfinished_write.pop_front();
            delete packet;
        }
        else
        {
            packet->packet_offset += write_len;
            break;
        }
    }
    return m_unfinished_write.empty();
}

void Connection::OnWriteEagain()
{
    WriteEagainEvent *event = new WriteEagainEvent;
    event->connection_fd = m_fd;
    Pass2MainThread(event);
}
