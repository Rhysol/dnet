#include "io_handler.h"
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>


IOHandler::IOHandler()
{

}

IOHandler::~IOHandler()
{

}

void IOHandler::Init(const CreateNetPacketFunc &create_packt_func, const OutputIOEventPipe &pipe)
{
    m_output_io_event_pipe = pipe;
    m_create_net_packet_func = create_packt_func;
}

void IOHandler::HandleListenEvent(const epoll_event &ev, int32_t listener_fd)
{
    if (ev.data.fd == listener_fd)
    {
        OnAccept(listener_fd);
    }
    else
    {
        HandleIOEvent(ev);
    }
}

void IOHandler::HandleIOEvent(const epoll_event &ev)
{
    if (ev.events & EPOLLIN)
    {
        OnRead(ev.data.fd);
    }
}

void IOHandler::OnAccept(int32_t listener_fd)
{
    sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(sockaddr_in));
    socklen_t len = sizeof(sockaddr);
	int client_fd = accept4(listener_fd, (sockaddr *)&client_addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
	if (client_fd == -1)
	{
		std::cout << "accept client failed! errno[" << errno << "]" << std::endl;
	}
	else
	{
        AcceptedConnectionEvent *event = new AcceptedConnectionEvent; //在net_manager内被删除
        event->connection_fd = client_fd;
        event->remote_ip = inet_ntoa(client_addr.sin_addr);
        event->remote_port = ntohs(client_addr.sin_port);
        m_output_io_event_pipe(event);
		std::cout << "accept client, ip[" << event->remote_ip << 
		"] port[" << event->remote_port << "]" << std::endl; 
	}
}

void IOHandler::OnDisconnect(int32_t connection_fd)
{
    DisconnectEvent *event = new DisconnectEvent;
    event->connection_fd = connection_fd;
    m_output_io_event_pipe(event);
}

void IOHandler::OnRead(int32_t connection_fd)
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
            OnDisconnect(connection_fd);
            return;
        }
        else if (read_len == -1)
        {
            return;
        }
    } while (true);
}

void IOHandler::ParseReadBuffer(int32_t connection_fd)
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

ReadEvent *IOHandler::GetUnfinishedReadEvent(int32_t connection_fd)
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

ReadEvent *IOHandler::CreateReadEvent(int32_t connection_fd)
{
    ReadEvent *event = new ReadEvent;
    event->packet = m_create_net_packet_func();
    event->packet->body_len = event->packet->ParseBodyLenFromHeader();
    event->connection_fd = connection_fd;
    return event;
}