#include "read_handler.h"
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

using namespace dnet;

ReadHandler::ReadHandler()
{

}

ReadHandler::~ReadHandler()
{

}

void ReadHandler::Init(const CreateNetPacketFunc &create_packt_func, const OutputIOEventPipe &pipe)
{
    m_output_io_event_pipe = pipe;
    m_create_net_packet_func = create_packt_func;
}

void ReadHandler::OnRead(int32_t connection_fd)
{
    int32_t read_len = -1;
    do {
        read_len = read(connection_fd, m_read_buffer.buffer, ReadBuffer::BufferMaxLen());
        if (read_len > 0)
        {
            m_read_buffer.buffer_len = read_len;
            ParseReadBuffer(connection_fd);
        }
        else if (read_len == 0)
        {
            OnUnexpectedDisconnect(connection_fd);
            return;
        }
        else if (read_len == -1)
        {
            return;
        }
    } while (true);
}

void ReadHandler::ParseReadBuffer(int32_t connection_fd)
{
    ReadEvent *event = GetUnfinishedReadEvent(connection_fd);
    if (event == nullptr)
    {
        event = CreateReadEvent(connection_fd);
    }
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
                m_unfinished_read.emplace(connection_fd, event);
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
                m_unfinished_read.emplace(connection_fd, event);
            }
            else
            {
                memcpy(packet->body + packet->body_offset, m_read_buffer.buffer + read_buffer_offset, len_to_read);
                packet->body_offset += len_to_read;
                read_buffer_offset += len_to_read;
                m_read_buffer.buffer_len -= len_to_read;
                m_output_io_event_pipe(event);
                event = CreateReadEvent(connection_fd);
                packet = event->packet;
            }
        }
    }
}

ReadEvent *ReadHandler::GetUnfinishedReadEvent(int32_t connection_fd)
{
    ReadEvent *event = nullptr;
    auto iter = m_unfinished_read.find(connection_fd);
    if (iter != m_unfinished_read.end())
    {
        event = iter->second;
        m_unfinished_read.erase(connection_fd);
    }
    return event;   
}

ReadEvent *ReadHandler::CreateReadEvent(int32_t connection_fd)
{
    ReadEvent *event = new ReadEvent;
    event->packet = m_create_net_packet_func();
    event->packet->body_len = event->packet->ParseBodyLenFromHeader();
    event->connection_fd = connection_fd;
    return event;
}

void ReadHandler::OnUnexpectedDisconnect(int32_t connection_fd)
{
    UnexpectedDisconnectEvent *event = new UnexpectedDisconnectEvent;
    event->connection_fd = connection_fd;
    m_output_io_event_pipe(event);
}

void ReadHandler::ClearUnfinishedRead(int32_t connection_fd)
{
    auto iter = m_unfinished_read.find(connection_fd);
    if (iter!= m_unfinished_read.end())
    {
        delete iter->second;
        m_unfinished_read.erase(connection_fd);
    }
}

void ReadHandler::OnCloseConnection(int32_t connection_fd)
{
    ClearUnfinishedRead(connection_fd);
}