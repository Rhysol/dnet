#include "io_thread.h"
#include <thread>


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
    m_sleep_duration.tv_nsec = 1000 * global_config.io_thread_sleep_duration; // 1ms
    m_keep_alive.store(true, std::memory_order_release);

    m_output_io_event_pipe = output_event_pipe;
    m_read_handler.Init(create_packet_func, std::bind(&IOThread::BeforeOutputIOEvent, this, std::placeholders::_1));
    m_write_handler.Init(std::bind(&IOThread::BeforeOutputIOEvent, this, std::placeholders::_1));
    return m_epoll_event_manager.Init(std::bind(&IOThread::HandleEpollEvent, this, std::placeholders::_1));
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
    while(m_keep_alive.load())
    {
        HandleIOEvent();

        if(m_epoll_event_manager.Update() == 0)
        {
            nanosleep(&m_sleep_duration, NULL);
        }
    }
    while(HandleIOEvent() != 0)
    {

    }
    std::cout << "thread:" << m_thread_id << " stoped" << std::endl;
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
            m_epoll_event_manager.MonitorFd(new_ev.data.fd, new_ev);
        }
    }
    else if (ev.events & EPOLLIN)
    {
        m_read_handler.OnRead(ev.data.fd);
    }
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
            { std::cout << "unkown event type:" << event->event_type << std::endl; }
            break;
        }
        delete event;//由io_event_pipe创建
        ++handle_count;
        if (handle_count >= global_config.io_thread_handle_io_event_num_of_one_update) break;
        event = m_io_events.Dequeue();
    }
    return handle_count;
}

void IOThread::OnRegisterConnection(const RegisterConnectionEvent &io_event)
{
    epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = io_event.connection_fd;
    m_epoll_event_manager.MonitorFd(io_event.connection_fd, ev);
    //todo, 是否需要立即进行read
    //...
}

void IOThread::OnWrite(const WriteEvent &event)
{
    if (m_epoll_event_manager.IsFdMonitored(event.connection_fd))
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
        m_epoll_event_manager.MonitorFd(io_event->connection_fd, ev);
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
    m_epoll_event_manager.StopMonitorFd(connection_fd);
    m_read_handler.OnCloseConnection(connection_fd);
    m_write_handler.OnCloseConnection(connection_fd);
}