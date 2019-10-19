#include "net_manager.h"
#include "listener_thread.h"


NetManager::NetManager()
{

}

NetManager::~NetManager()
{
    for (auto &thread : m_io_threads)
    {
        thread->Stop();
        thread->Join();
        delete thread;
    }
}

bool NetManager::Init(uint16_t thread_num, const std::string &listen_ip, uint16_t listen_port,
    const CreateNetPacketFunc &create_packet_func, const HandlePacketFunc &handle_packet_func)
{
    m_io_threads_num = thread_num; 
    m_events_queue.Init(m_io_threads_num),
    m_io_event_pipe.Init(m_io_threads_num, std::bind(&NetManager::AcceptIOEvent, this,
        std::placeholders::_1, std::placeholders::_2), create_packet_func);
    m_handle_packet_func = handle_packet_func;

    InitThreads(listen_ip, listen_port);

}

void NetManager::InitThreads(const std::string &listen_ip, uint16_t listen_port)
{
    m_io_threads.push_back(new ListenerThread);
    m_listener_thread = dynamic_cast<ListenerThread *>(m_io_threads[0]);
    m_listener_thread->Init(0, listen_ip, listen_port, 
        std::bind(&IOEventPipe::OnAccept, &m_io_event_pipe, std::placeholders::_1, std::placeholders::_2),
        std::bind(&IOEventPipe::OnRead, &m_io_event_pipe, std::placeholders::_1, std::placeholders::_2));
    
    IOThread *io_thread = nullptr;
    for (uint16_t i = 1; i < m_io_threads_num; i++)
    {
        io_thread = new IOThread;
        m_io_threads.push_back(io_thread);
        m_io_threads.back()->Init(i, std::bind(&IOEventPipe::OnRead, &m_io_event_pipe, 
            std::placeholders::_1, std::placeholders::_2));
    }

    for (auto &thread : m_io_threads)
    {
        thread->Start();
    }
}

void NetManager::Stop()
{
    for (auto &thread : m_io_threads)
    {
        thread->Stop();
        thread->Join();
    }
    m_connection_manager.CloseAllConnections();
    m_keep_alive = false;
    std::cout << "io thread stoped" << std::endl;
}

uint32_t NetManager::Update()
{
    uint16_t one_turn_handle_num = 100;
    IOEvent *event = m_events_queue.Dequeue();
    uint16_t handle_count = 0;
    while (event != nullptr)
    {
        switch (event->event_type)
        {
        case IOEvent::EventType::ACCEPTED_CONNECTION:
            HandleAcceptedConnectionEvent((AcceptedConnectionEvent &)(*event));
            break;
        case IOEvent::EventType::READ:
            HandleReadEvent((ReadEvent &)(*event));
            break;
        default:
            { std::cout << "unkown event type" << std::endl; }
            break;
        }
        delete event;//由io_event_pipe创建
        ++handle_count;
        if (handle_count >= one_turn_handle_num) break;
        event = m_events_queue.Dequeue();
    }
    return handle_count;
}

void NetManager::HandleAcceptedConnectionEvent(const AcceptedConnectionEvent &event)
{
    const Connection *connection = m_connection_manager.AddConnection(event.connection_fd, 
        event.remote_ip, event.remote_port);
    if (connection == nullptr) return;
    uint16_t io_thread_id = HashToIoThread(connection->fd);
    m_io_threads[io_thread_id]->RegisterConnectionFd(connection->fd);
}

void NetManager::HandleReadEvent(const ReadEvent &event)
{
    //m_handle_packet_func(*event.packet);
}

const Connection *NetManager::ConnectTo(const std::string &remote_ip, uint16_t remote_port)
{
    const Connection *connection = m_connection_manager.ConnectTo(remote_ip, remote_port);
    if (connection != nullptr)
    {
        uint16_t io_thread_id = HashToIoThread(connection->fd);
        m_io_threads[io_thread_id]->RegisterConnectionFd(connection->fd);
    }
    return connection;
}

void NetManager::AcceptIOEvent(IOEvent *event, uint16_t thread_id)
{
    m_events_queue.Enqueue(event, thread_id);
}

uint16_t NetManager::HashToIoThread(int32_t connection_fd)
{
    uint16_t thread_id = connection_fd % (int32_t)m_io_threads_num;
    return thread_id;
}