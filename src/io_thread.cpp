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
}

bool IOThread::Init(uint16_t thread_id, const CreateNetPacketFunc &create_packet_func, const NetConfig *net_config)
{
    m_net_config = net_config;
    m_create_packet_func = create_packet_func;
    m_thread_id = thread_id;
    m_sleep_duration.tv_sec = 0;
    m_sleep_duration.tv_nsec = 1000 * m_net_config->io_thread_sleep_duration; // 1ms
    m_keep_alive.store(true, std::memory_order_release);

	m_epoll_fd = epoll_create1(EPOLL_CLOEXEC);
	m_events = new epoll_event[m_net_config->epoll_max_event_num];
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
}

void IOThread::Update()
{
    int epoll_event_num = 0;
    while(m_keep_alive.load())
    {
        HandleIOEvent();
		epoll_event_num = epoll_wait(m_epoll_fd, m_events, m_net_config->epoll_max_event_num, 0);
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
			HandleEpollEvent(m_events[i]);
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
        if (connection.HandleUnfinishedWrite())
        {
            connection.Read();
            EpollCtl(ev.data.fd, EPOLLIN | EPOLLET, EPOLL_CTL_MOD);
        }
    }
    else if (ev.events & EPOLLIN)
    {
        connection.Read();
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
    auto result = m_connections.emplace(io_event.connection_fd, Connection());
    if (!result.second)
    {
        LOGE("fd {} already exist!", io_event.connection_fd);
        return;
    }
    Connection &connection = result.first->second;
    connection.Init(io_event.connection_fd, m_net_config, m_create_packet_func);
    connection.SetNextPasser(this, IOEventPasser::EDestination::MAIN_THREAD);
    EpollCtl(io_event.connection_fd, EPOLLIN | EPOLLET, EPOLL_CTL_ADD);

}

void IOThread::OnWrite(WriteEvent &event)
{
    auto iter = m_connections.find(event.connection_fd);
    if (iter != m_connections.end())
    {
        iter->second.Write(event.packet);
    }
}

void IOThread::Pass2MainThread(IOEvent *io_event)
{
    if (io_event->event_type == IOEvent::EventType::UNEXPECTED_DISCONNECT)
    {
        CloseConnection(io_event->connection_fd);
        IOEventPasser::Pass2MainThread(io_event);
    }
    else if (io_event->event_type == IOEvent::EventType::WRITE_EAGAIN)
    {
        EpollCtl(io_event->connection_fd, EPOLLOUT | EPOLLET, EPOLL_CTL_MOD);
        delete io_event;
    }
}

void IOThread::Pass2IOThread(IOEvent *io_event)
{
    m_io_events.Enqueue(io_event);
}

void IOThread::CloseConnection(int32_t connection_fd)
{
    close(connection_fd);
	epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, connection_fd, NULL);
    m_connections.erase(connection_fd);
}

void IOThread::CloseAllConnection()
{
    int32_t fd = -1;
    for (auto &iter : m_connections)
    {
        fd = iter.second.GetConnectionFD();
        close(fd);
        epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    }
}