#include "io_thread.h"
#include <thread>
#include "logger.h"
#include <sys/socket.h>

using namespace dnet;

IOThread::IOThread()
{
}

IOThread::~IOThread()
{
    if (m_thread != nullptr)
    {
        delete m_thread;
    }
    if (m_epoll_events)
    {
        delete[] m_epoll_events;
    }
}

bool IOThread::Init(uint16_t thread_id, const NetConfig *net_config)
{
    m_net_config = net_config;
    m_thread_id = thread_id;
    m_sleep_duration.tv_sec = 0;
    m_sleep_duration.tv_nsec = 1000 * m_net_config->io_thread_sleep_duration; // 1ms
    m_keep_alive.store(true, std::memory_order_release);

	m_epoll_fd = epoll_create1(EPOLL_CLOEXEC);
	m_epoll_events = new epoll_event[m_net_config->epoll_max_event_num];
	return true;
}

void IOThread::Start()
{
    m_thread = new std::thread(std::bind(&IOThread::Update, this));
}

void IOThread::Stop()
{
    m_keep_alive.store(false, std::memory_order_release);
}

void IOThread::Join()
{
    if(m_thread && m_thread->joinable())
    {
        m_thread->join();
    }
}

void IOThread::Update()
{
    int epoll_event_num = 0;
    while(m_keep_alive.load())
    {
        for (uint64_t to_close_id : m_connections_to_close)
        {
            RemoveConnection(to_close_id);
        }
        m_connections_to_close.clear();
        HandleIOEvent();
		epoll_event_num = epoll_wait(m_epoll_fd, m_epoll_events, m_net_config->epoll_max_event_num, 0);
		if (epoll_event_num == -1)
		{
			LOGE("epoll_wait failed!");
            break;
		}
        else if (epoll_event_num == 0)
        {
            nanosleep(&m_sleep_duration, NULL);
            continue;
        }
		for (int i = 0; i < epoll_event_num; i++)
		{
			HandleEpollEvent(m_epoll_events[i]);
		}
    }
    while(HandleIOEvent() != 0) {}
    RemoveAllConnection();
    LOGI("thread: {} stopped!", m_thread_id);
}

void IOThread::HandleEpollEvent(const epoll_event &ev)
{
    auto iter = m_fd_2_connection.find(ev.data.fd);
    if (iter == m_fd_2_connection.end())
    {
        LOGE("can't find fd {}", ev.data.fd);
        return;
    }
    Connection &connection = *iter->second;
    if (ev.events & EPOLLERR)
    {
        HandleEPOLLERR(ev.data.fd, connection);
    }
    else if (ev.events & EPOLLRDHUP)
    {
        connection.OnUnexpectedDisconnect();
    }
    else
    {
        if (ev.events & EPOLLIN)
        {
            connection.Receive();
        }
        if (ev.events &EPOLLOUT)
        {
            connection.Connected() ?
                connection.SendRemainPacket() :
                HandleAsyncConnectResult(connection, true);
        }
    }
    
}

void IOThread::HandleEPOLLERR(int32_t fd, Connection &connection)
{
    int error = 0;
    socklen_t len = sizeof(error);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (void *)&error, &len) == 0)
    {
        LOGE("on epoll err, fd {}, error {}, error_str: {}, connected {}", fd, error, strerror(error), connection.Connected());
    }
    else
    {
        LOGE("on epoll err, fd {},connected {}", fd, connection.Connected());
    }
    if (connection.Connected())
    {
        connection.OnUnexpectedDisconnect();
    }
    else
    {
        HandleAsyncConnectResult(connection, false);
    }
}

void IOThread::HandleAsyncConnectResult(Connection &connection, bool is_success)
{
    uint64_t connection_id = connection.GetID();
    if (is_success && EpollCtl(connection.GetFD(), EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLET, EPOLL_CTL_MOD))
    {
        connection.SetConnected(true);
    }
    else
    {
        is_success = false;
        RemoveConnection(connection_id);
    }
    io_event::AsyncConnectResult *event = new io_event::AsyncConnectResult;
    event->connection_id = connection_id;
    event->is_success = is_success;
    Pass2MainThread(event);
}

bool IOThread::EpollCtl(int32_t fd, uint32_t events, int32_t operation)
{
    epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    if (epoll_ctl(m_epoll_fd, operation, ev.data.fd, &ev) != 0)
    {
        LOGE("epoll_ctl fd:{} failed! operation: {}, events:{}, errno: {}", fd, operation, events, errno);
        return false;
    }
    return true;
}

uint32_t IOThread::HandleIOEvent()
{
    io_event::IOEvent *event = m_io_events.Dequeue();
    uint16_t handle_count = 0;
    while (event != nullptr)
    {
        switch (event->event_type)
        {
        case io_event::EventType::REGISTER_CONNECTION:
            OnRegisterConnection(*(io_event::RegisterConnection *)event);
            break;
        case io_event::EventType::SEND_A_PACKET:
            OnSendAPacket(*(io_event::SendAPacket *)event);
            break;
        case io_event::EventType::CLOSE_CONNECTION_REQUEST:
            RemoveConnection(event->connection_id);
            break;
        default:
            { LOGE("unkown event type: {}", (int16_t)event->event_type); }    
            break;
        }
        delete event;
        ++handle_count;
        if (handle_count >= m_net_config->io_thread_handle_io_event_num_of_one_update) break;
        event = m_io_events.Dequeue();
    }
    return handle_count;
}

void IOThread::OnRegisterConnection(const io_event::RegisterConnection &io_event)
{
    Connection &connection = m_id_2_connection[io_event.connection_id];
    if (connection.HasInited())
    {
        LOGE("id {} already exist!", io_event.connection_id);
        return;
    }
    m_fd_2_connection[io_event.connection_fd] = &connection;
    connection.Init(io_event.connection_id, io_event.connection_fd, m_net_config);
    connection.SetNextPasser(this, IOEventPasser::EDestination::MAIN_THREAD);
    if (io_event.is_async)
    {
        if (!EpollCtl(io_event.connection_fd, EPOLLOUT | EPOLLERR , EPOLL_CTL_ADD))
        {
            HandleAsyncConnectResult(connection, false);
            LOGE("register async connection: {} to epoll failed", io_event.connection_id);
        }
    }
    else
    {
        connection.SetConnected(true);
        if (!EpollCtl(io_event.connection_fd, EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLET, EPOLL_CTL_ADD))
        {
            connection.OnUnexpectedDisconnect();
        }
    }
}

void IOThread::OnSendAPacket(io_event::SendAPacket &event)
{
    auto iter = m_id_2_connection.find(event.connection_id);
    if (iter != m_id_2_connection.end())
    {
        iter->second.Send(event.packet_bytes);
    }
}

void IOThread::Pass2MainThread(io_event::IOEvent *io_event)
{
    if (io_event->event_type == io_event::EventType::UNEXPECTED_DISCONNECT)
    {
        m_connections_to_close.push_back(io_event->connection_id);
    }
    io_event->source_thread_id = m_thread_id;
    IOEventPasser::Pass2MainThread(io_event);
}

void IOThread::Pass2IOThread(io_event::IOEvent *io_event)
{
    m_io_events.Enqueue(io_event);
}

void IOThread::RemoveConnection(uint64_t connection_id)
{
    auto iter = m_id_2_connection.find(connection_id);
    if (iter == m_id_2_connection.end()) return;
    iter->second.SendRemainPacket();
    close(iter->second.GetFD());
    m_fd_2_connection.erase(iter->second.GetFD());
    m_id_2_connection.erase(iter);
}

void IOThread::RemoveAllConnection()
{
    int32_t fd = -1;
    for (auto &iter : m_id_2_connection)
    {
        iter.second.SendRemainPacket();
        fd = iter.second.GetFD();
        close(fd);
    }
}