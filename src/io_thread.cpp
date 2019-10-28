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

bool IOThread::Init(uint16_t thread_id, const IOHandler::CreateNetPacketFunc &create_packet_func,
    const IOHandler::OutputIOEventPipe &output_event_pipe)
{
    m_thread_id = thread_id;
    m_sleep_interval.tv_sec = 0;
    m_sleep_interval.tv_nsec = 1000 * 1000; // 1ms

    m_epoll_event_handler.Init(create_packet_func, output_event_pipe);
    return m_epoll_event_manager.Init(std::bind(&IOHandler::HandleIOEvent, m_epoll_event_handler, std::placeholders::_1));
}

void IOThread::Start()
{
    m_thread = new std::thread(std::bind(&IOThread::Update, this));
}

void IOThread::Stop()
{
    m_keep_alive = false;
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
    while(m_keep_alive)
    {
        if(m_epoll_event_manager.Update() == 0)
        {
            nanosleep(&m_sleep_interval, NULL);
        }
    }
    std::cout << "thread:" << m_thread_id << " stoped" << std::endl;
}

void IOThread::RegisterConnectionFd(int32_t fd)
{
    epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = fd;
    m_epoll_event_manager.MonitorFd(fd, ev);
    //todo,立即进行read
    //...
}