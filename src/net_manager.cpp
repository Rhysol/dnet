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
    NetHandlerInterface *net_hander)
{
    if (net_hander == nullptr) return false;
    m_io_threads_num = thread_num; 
    m_net_handler = net_hander;
    m_events_queue.Init(m_io_threads_num),
    InitThreads(listen_ip, listen_port);
    return true;
}

void NetManager::InitThreads(const std::string &listen_ip, uint16_t listen_port)
{
    m_io_threads.push_back(new ListenerThread);
    m_listener_thread = dynamic_cast<ListenerThread *>(m_io_threads[0]);
    m_listener_thread->Init(0, listen_ip, listen_port, 
        std::bind(&NetHandlerInterface::CreateNetPacket, m_net_handler),
        std::bind(&NetManager::AcceptIOEvent, this, std::placeholders::_1, 0));
    
    IOThread *io_thread = nullptr;
    for (uint16_t i = 1; i < m_io_threads_num; i++)
    {
        io_thread = new IOThread;
        m_io_threads.push_back(io_thread);
        m_io_threads.back()->Init(i, std::bind(&NetHandlerInterface::CreateNetPacket, m_net_handler),
            std::bind(&NetManager::AcceptIOEvent, this, std::placeholders::_1, i));    
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
        case IOEvent::EventType::ACCEPT_CONNECTION:
            OnAcceptConnection((AcceptConnectionEvent &)(*event));
            break;
        case IOEvent::EventType::READ:
            OnRead((ReadEvent &)(*event));
            break;
        case IOEvent::EventType::CLOSE_CONNECTION_COMPLETE:
            OnCloseConnectionComplete((CloseConnectionCompleteEvent &)(*event));
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

//从listener thread接收的新连接
void NetManager::OnAcceptConnection(const AcceptConnectionEvent &event)
{
    const Connection *connection = m_connection_manager.AddConnection(event.connection_fd, 
        event.remote_ip, event.remote_port);
    if (connection == nullptr) return;
    uint16_t io_thread_id = HashToIoThread(connection->fd);
    RegisterConnectionEvent *to_thread_event = new RegisterConnectionEvent;
    to_thread_event->connection_fd = event.connection_fd; 
    m_io_threads[io_thread_id]->AcceptIOEvent(to_thread_event);
}

void NetManager::OnRead(const ReadEvent &event)
{
    //m_net_handler->HandlePacket(*event.packet);
}

void NetManager::OnCloseConnectionComplete(const CloseConnectionCompleteEvent &event)
{
    m_connection_manager.DisconnectFrom(event.connection_fd);
    // m_net_handler->HandleDisconnect(event.connection_fd);
}

const Connection *NetManager::ConnectTo(const std::string &remote_ip, uint16_t remote_port)
{
    const Connection *connection = m_connection_manager.ConnectTo(remote_ip, remote_port);
    if (connection != nullptr)
    {
        uint16_t io_thread_id = HashToIoThread(connection->fd);
        RegisterConnectionEvent *to_thread_event = new RegisterConnectionEvent;
        to_thread_event->connection_fd = connection->fd; 
        m_io_threads[io_thread_id]->AcceptIOEvent(to_thread_event);
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