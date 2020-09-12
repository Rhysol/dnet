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
    if (m_incomplete_receive)
    {
        delete m_incomplete_receive;
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
            m_read_buffer.remain_len = read_len;
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
    uint32_t len_to_read = 0;
    do {
        std::vector<char> &packet_bytes = GetInCompletePacketBytes();
        len_to_read = std::min<uint32_t>(m_read_buffer.remain_len, packet_bytes.capacity() - m_received_len);
        memcpy(packet_bytes.data() + m_received_len, m_read_buffer.buffer + m_read_buffer.offset, len_to_read);
        m_read_buffer.remain_len -= len_to_read;
        m_read_buffer.offset += len_to_read;
        m_received_len += len_to_read;
        
        if (m_received_len == packet_bytes.capacity())
        {
            Pass2MainThread(m_incomplete_receive);
            m_incomplete_receive = nullptr;
            m_received_len = 0;
        }
    } while (m_read_buffer.remain_len != 0);
    m_read_buffer.offset = 0;
}

std::vector<char> &Connection::GetInCompletePacketBytes()
{
    uint32_t header_len = m_net_config->packet_header_len;
    uint32_t packet_len = 0;
    if (nullptr == m_incomplete_receive)
    {
        packet_len = header_len;
        if (m_read_buffer.remain_len >= header_len)
        {
            packet_len += m_net_config->get_body_len(m_read_buffer.buffer + m_read_buffer.offset);
        }
        m_incomplete_receive = new io_event::ReceiveAPacket(packet_len);
        m_incomplete_receive->connection_id = m_id;
    }
    else if (m_received_len < header_len && m_received_len + m_read_buffer.remain_len >= header_len)
    {
        memcpy(m_incomplete_receive->packet_bytes.data() + m_received_len, 
            m_read_buffer.buffer + m_read_buffer.offset, 
            header_len - m_received_len);
        packet_len = header_len + m_net_config->get_body_len(m_incomplete_receive->packet_bytes.data());
        m_incomplete_receive->packet_bytes.resize(packet_len);
    }
    return m_incomplete_receive->packet_bytes;
}

void Connection::OnUnexpectedDisconnect()
{
    if (m_to_close) return;
    m_to_close = true;
    io_event::IOEvent *event = new io_event::IOEvent(io_event::EventType::UNEXPECTED_DISCONNECT);
    event->connection_id = m_id;
    Pass2MainThread(event);
}

void Connection::Send(std::vector<char> &packet_bytes)
{
    m_packet_to_send.push_back(std::move(packet_bytes));
    if (!m_connected) return;
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
}

void Connection::SendRemainPacket()
{
    if (!m_connected) return;
    m_can_send = true;
    DoSendRemainPacket();
}

void Connection::DoSendRemainPacket()
{
    std::vector<char> *packet_bytes = nullptr;
    const char *bytes_to_write = 0;
    int32_t len_to_write = 0;
    int32_t write_len = 0;
    while (m_packet_to_send.size() != 0)
    {
        packet_bytes = &m_packet_to_send.front();
        bytes_to_write = packet_bytes->data() + m_send_offset;
        len_to_write = packet_bytes->size() - m_send_offset;
        write_len = send(m_fd, bytes_to_write, len_to_write, MSG_NOSIGNAL);
        if (write_len == -1)
        {
            if (EAGAIN != errno)
            {
                OnUnexpectedDisconnect();
            }
            m_can_send = false;
            break;
        }
        else if (len_to_write == write_len)
        {
            m_packet_to_send.pop_front();
            m_send_offset = 0;
        }
        else
        {
            m_send_offset += write_len;
        }
    }
}
