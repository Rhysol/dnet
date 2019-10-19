#include "io_event_pipe.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <sstream>

IOEventPipe::IOEventPipe()
{
}

IOEventPipe::~IOEventPipe()
{
    if (m_unfinished_read != nullptr)
    {
        for (uint16_t i = 0; i < m_thread_num; i++)
        {
            std::unordered_map<int32_t, ReadEvent *> &events_of_thread = m_unfinished_read[i];
            for (auto &iter : events_of_thread)
            {
                delete iter.second;
            }
        }
        delete[] m_unfinished_read;
    }
    if (m_thread_read_buffer != nullptr)
    {
        delete[] m_thread_read_buffer;
    }
}

bool IOEventPipe::Init(uint16_t thread_num, const AcceptEventFunc &accept_event_func, const CreateNetPacketFunc &create_packet_func)
{
    if (!accept_event_func || !create_packet_func || thread_num == 0) return false;
    m_thread_num = thread_num;
    m_unfinished_read = new std::unordered_map<int32_t, ReadEvent *>[m_thread_num]();
    m_thread_read_buffer = new ThreadReadBuffer[thread_num];
    m_accept_event_func = accept_event_func;
    m_create_net_packet_func = create_packet_func;

    return true;
}

void IOEventPipe::OnAccept(int32_t listener_fd, uint16_t thread_id)
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
        m_accept_event_func(event, thread_id);
		std::cout << "accept client, ip[" << event->remote_ip << 
		"] port[" << event->remote_port << "]" << std::endl; 
	}

}

void IOEventPipe::OnRead(int32_t connection_fd, uint16_t thread_id)
{
    ThreadReadBuffer &thread_buffer = m_thread_read_buffer[thread_id];
    int32_t read_len = -1;
    do {
        read_len = read(connection_fd, thread_buffer.buffer, ThreadReadBuffer::BufferMaxLen());
        if (read_len > 0)
        {
            thread_buffer.buffer_len = read_len;
            ParseReadBuffer(connection_fd, thread_id);
        }
        else if (read_len == 0)
        {
            OnDisconnect(connection_fd, thread_id);
            return;
        }
        else if (read_len == -1)
        {
            return;
        }
    } while (true);
}

void IOEventPipe::ParseReadBuffer(int32_t connection_fd, uint16_t thread_id)
{
    ThreadReadBuffer &thread_buffer = m_thread_read_buffer[thread_id];
    ReadEvent *event = GetUnfinishedReadEvent(connection_fd, thread_id);
    if (event == nullptr)
    {
        event = CreateReadEvent(connection_fd, thread_id);
    }
    NetPacketInterface *packet = event->packet;

    int32_t len_to_read = 0;
    int32_t thread_buffer_offset = 0;
    while (thread_buffer.buffer_len != 0)
    {
        //读取包头
        if (packet->header_offset < packet->header_len)
        {
            len_to_read = packet->header_len - packet->header_offset;
            if (thread_buffer.buffer_len < len_to_read)
            {
                memcpy(packet->header + packet->header_offset, thread_buffer.buffer + thread_buffer_offset, thread_buffer.buffer_len);
                packet->header_offset += thread_buffer.buffer_len;
                thread_buffer_offset += thread_buffer.buffer_len;
                thread_buffer.buffer_len = 0;
                m_unfinished_read[thread_id].emplace(connection_fd, event);
            }
            else
            {
                memcpy(packet->header + packet->header_offset, thread_buffer.buffer + thread_buffer_offset, len_to_read);
                packet->header_offset += len_to_read;
                thread_buffer_offset += len_to_read;
                thread_buffer.buffer_len -= len_to_read;
                packet->body_len = packet->ParseBodyLenFromHeader();
                packet->body = new char[packet->body_len];
            }
        }
        else //读取body
        {
            len_to_read = packet->body_len - packet->body_offset;
            if (thread_buffer.buffer_len < len_to_read)
            {
                memcpy(packet->body + packet->body_offset, thread_buffer.buffer + thread_buffer_offset, thread_buffer.buffer_len);
                packet->body_offset += thread_buffer.buffer_len;
                thread_buffer_offset += thread_buffer.buffer_len;
                thread_buffer.buffer_len = 0;
                m_unfinished_read[thread_id].emplace(connection_fd, event);
            }
            else
            {
                memcpy(packet->body + packet->body_offset, thread_buffer.buffer + thread_buffer_offset, len_to_read);
                packet->body_offset += len_to_read;
                thread_buffer_offset += len_to_read;
                thread_buffer.buffer_len -= len_to_read;
                m_accept_event_func(event, thread_id);
                event = CreateReadEvent(connection_fd, thread_id);
                packet = event->packet;
            }
        }
    }
}

void IOEventPipe::OnDisconnect(int32_t connection_fd, uint16_t thread_id)
{
    DisconnectEvent *event = new DisconnectEvent;
    event->connection_fd = connection_fd;
    m_accept_event_func(event, thread_id);
}

ReadEvent *IOEventPipe::GetUnfinishedReadEvent(int32_t connection_fd, uint16_t thread_id)
{
    ReadEvent *event = nullptr;
    std::unordered_map<int32_t, ReadEvent *> &unfinished_read_of_thread = m_unfinished_read[thread_id];
    auto iter = unfinished_read_of_thread.find(connection_fd);
    if (iter != unfinished_read_of_thread.end())
    {
        event = iter->second;
        unfinished_read_of_thread.erase(connection_fd);
    }
    return event;   
}

ReadEvent *IOEventPipe::CreateReadEvent(int32_t connection_fd, uint16_t thread_id)
{
    ReadEvent *event = new ReadEvent;
    event->packet = m_create_net_packet_func();
    event->packet->body_len = event->packet->ParseBodyLenFromHeader();
    event->connection_fd = connection_fd;
    return event;
}
