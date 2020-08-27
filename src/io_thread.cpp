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

bool IOThread::Init(uint16_t thread_id, const ReadHandler::CreateNetPacketFunc &create_packet_func,
    const OutputIOEventPipe &output_event_pipe)
{
    m_thread_id = thread_id;
    m_sleep_duration.tv_sec = 0;
    m_sleep_duration.tv_nsec = 1000 * m_net_config.io_thread_sleep_duration; // 1ms
    m_keep_alive.store(true, std::memory_order_release);

    m_output_io_event_pipe = output_event_pipe;
    m_read_handler.Init(create_packet_func, std::bind(&IOThread::BeforeOutputIOEvent, this, std::placeholders::_1));
    m_write_handler.Init(std::bind(&IOThread::BeforeOutputIOEvent, this, std::placeholders::_1));

	m_epoll_fd = epoll_create1(EPOLL_CLOEXEC);
	m_events = new epoll_event[m_net_config.epoll_max_event_num];
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
		epoll_event_num = epoll_wait(m_epoll_fd, m_events, m_net_config.epoll_max_event_num, 0);
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
    LOGI("thread: {} stopped!", m_thread_id);
}

void IOThread::HandleEpollEvent(const epoll_event &ev)
{
    if (ev.events & EPOLLOUT)
    {
        if (m_write_handler.HandleUnfinishedPacket(ev.data.fd))
        {
            m_read_handler.OnRead(ev.data.fd);
            epoll_event new_ev;
            new_ev.data.fd = ev.data.fd;
            new_ev.events = EPOLLIN | EPOLLET;
            MonitorFd(new_ev.data.fd, new_ev);
        }
    }
    else if (ev.events & EPOLLIN)
    {
        m_read_handler.OnRead(ev.data.fd);
    }
}

bool IOThread::MonitorFd(int32_t fd, epoll_event &event)
{
	int operation = EPOLL_CTL_MOD;
	if (m_monitoring_fd.find(fd) == m_monitoring_fd.end())
	{
		m_monitoring_fd.emplace(fd);
		operation = EPOLL_CTL_ADD;
	}

	if (epoll_ctl(m_epoll_fd, operation, fd, &event) != 0)
	{
		LOGE("epoll_ctl failed! operation: {}, errno: {}", operation, errno);
		return false;
	}
	return true;
}

void IOThread::StopMonitorFd(int32_t fd)
{
	m_monitoring_fd.erase(fd);
	epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, fd, NULL);
}

bool IOThread::IsFdMonitored(int32_t fd)
{
	return m_monitoring_fd.find(fd) != m_monitoring_fd.end();
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
            OnCloseConnectionRequest((CloseConnectionRequestEvent &)(*event));
            break;
        default:
            { LOGE("unkown event type: {}", (int16_t)event->event_type); }    
            break;
        }
        delete event;//由io_event_pipe创建
        ++handle_count;
        if (handle_count >= m_net_config.io_thread_handle_io_event_num_of_one_update) break;
        event = m_io_events.Dequeue();
    }
    return handle_count;
}

void IOThread::OnRegisterConnection(const RegisterConnectionEvent &io_event)
{
    epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = io_event.connection_fd;
    MonitorFd(io_event.connection_fd, ev);
}

void IOThread::OnWrite(const WriteEvent &event)
{
    if (IsFdMonitored(event.connection_fd))
    {
        m_write_handler.Send(event.packet);
    }
}

void IOThread::OnCloseConnectionRequest(const CloseConnectionRequestEvent &event)
{
    CloseConnection(event.connection_fd);
}

void IOThread::AcceptIOEvent(IOEvent *event)
{
    m_io_events.Enqueue(event);
}

void IOThread::BeforeOutputIOEvent(IOEvent *io_event)
{
    if (io_event->event_type == IOEvent::EventType::UNEXPECTED_DISCONNECT)
    {
        CloseConnection(io_event->connection_fd);
        CloseConnectionCompleteEvent *event = new CloseConnectionCompleteEvent;
        event->connection_fd = io_event->connection_fd;
        m_output_io_event_pipe(event);
        delete io_event;
    }
    else if (io_event->event_type == IOEvent::EventType::WRITE_EAGAIN)
    {
        epoll_event ev;
        ev.events = EPOLLOUT | EPOLLET;
        ev.data.fd = io_event->connection_fd;
        MonitorFd(io_event->connection_fd, ev);
        delete io_event;
    }
    else
    {
        m_output_io_event_pipe(io_event);
    }
}

void IOThread::CloseConnection(int32_t connection_fd)
{
    close(connection_fd);
    StopMonitorFd(connection_fd);
    m_read_handler.OnCloseConnection(connection_fd);
    m_write_handler.OnCloseConnection(connection_fd);
}