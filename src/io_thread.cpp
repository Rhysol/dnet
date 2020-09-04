#include "io_thread.h"
#include <thread>
#include "logger.h"

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
    if(m_thread->joinable())
    {
        m_thread->join();
    }
    else
    {
        LOGE("join twice");
    }
}

void IOThread::Update()
{
    int epoll_event_num = 0;
    while(m_keep_alive.load())
    {
        for (int32_t to_close_fd : m_connections_to_close)
        {
            CloseConnection(to_close_fd);
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
    while(HandleIOEvent() != 0)
    {

    }
    CloseAllConnection();
    LOGI("thread: {} stopped!", m_thread_id);
}

void IOThread::HandleEpollEvent(const epoll_event &ev)
{
    auto iter = m_connections.find(ev.data.fd);
    if (iter == m_connections.end())
    {
        LOGE("can't find fd {}", ev.data.fd);
    }
    Connection &connection = iter->second;
    if (ev.events & EPOLLOUT)
    {
        if (connection.SendRemainPacket())
        {
            connection.Receive();
            EpollCtl(ev.data.fd, EPOLLIN | EPOLLET, EPOLL_CTL_MOD);
        }
    }
    else if (ev.events & EPOLLIN)
    {
        connection.Receive();
    }
}

bool IOThread::EpollCtl(int32_t fd, uint32_t events, int32_t operation)
{
    epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    if (epoll_ctl(m_epoll_fd, operation, ev.data.fd, &ev) != 0)
    {
        LOGE("epoll_ctl mod failed! operation: {}, errno: {}", operation, errno);
        return false;
    }
    return true;
}

uint32_t IOThread::HandleIOEvent()
{
    IOEvent *event = m_io_events.Dequeue();
    uint16_t handle_count = 0;
    while (event != nullptr)
    {
        switch (event->event_type)
        {
        case IOEvent::EventType::REGISTER_CONNECTION:
            OnRegisterConnection((RegisterConnectionEvent &)(*event));
            break;
        case IOEvent::EventType::WRITE:
            OnWrite((WriteEvent &)(*event));
            break;
        case IOEvent::EventType::CLOSE_CONNECTION_REQUEST:
            CloseConnection(event->connection_fd);
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

void IOThread::OnRegisterConnection(const RegisterConnectionEvent &io_event)
{
    Connection &connection = m_connections[io_event.connection_fd];
    if (connection.HasInited())
    {
        LOGE("fd {} already exist!", io_event.connection_fd);
        return;
    }
    connection.Init(io_event.connection_fd, m_net_config);
    connection.SetNextPasser(this, IOEventPasser::EDestination::MAIN_THREAD);
    EpollCtl(io_event.connection_fd, EPOLLIN | EPOLLET, EPOLL_CTL_ADD);

}

void IOThread::OnWrite(WriteEvent &event)
{
    auto iter = m_connections.find(event.connection_fd);
    if (iter != m_connections.end())
    {
        iter->second.Send(event.packet);
    }
}

void IOThread::OnCloseConnectionRequest(const CloseConnectionRequestEvent &event)
{
    CloseConnection(event.connection_fd);
    CloseConnectionCompleteEvent *event_to_main_thread = new CloseConnectionCompleteEvent;
    event_to_main_thread->connection_fd = event.connection_fd;
    Pass2MainThread(event_to_main_thread);
}

void IOThread::Pass2MainThread(IOEvent *io_event)
{
    if (io_event->event_type == IOEvent::EventType::WRITE_EAGAIN)
    {
        EpollCtl(io_event->connection_fd, EPOLLOUT | EPOLLET, EPOLL_CTL_MOD);
        delete io_event;
        return;
    }
    if (io_event->event_type == IOEvent::EventType::UNEXPECTED_DISCONNECT)
    {
        m_connections_to_close.push_back(io_event->connection_fd);
    }
    io_event->source_thread_id = m_thread_id;
    IOEventPasser::Pass2MainThread(io_event);
}

void IOThread::Pass2IOThread(IOEvent *io_event)
{
    m_io_events.Enqueue(io_event);
}

void IOThread::CloseConnection(int32_t connection_fd)
{
    auto iter = m_connections.find(connection_fd);
    if (iter == m_connections.end()) return;
    iter->second.SendRemainPacket();
    close(connection_fd);
	epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, connection_fd, NULL);
    m_connections.erase(iter);
}

void IOThread::CloseAllConnection()
{
    int32_t fd = -1;
    for (auto &iter : m_connections)
    {
        iter.second.SendRemainPacket();
        fd = iter.second.GetConnectionFD();
        close(fd);
        epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    }
}