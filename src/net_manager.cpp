#include "net_manager.h"
#include "listener_thread.h"
#include <cstring>
#include "net_config.h"
#include "logger.h"
#include <spdlog/sinks/hourly_file_sink.h>
#include <spdlog/async.h>

using namespace dnet;

NetConfig dnet::global_config;
std::shared_ptr<spdlog::logger> dnet::net_logger;

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

bool NetManager::Init(const NetConfig &config, NetEventInterface *net_hander)
{
    if (net_hander == nullptr) return false;
    InitLogger();
    global_config = config;
    m_net_event_handler = net_hander;
    m_events_queue.Init(global_config.io_thread_num),
    InitThreads(global_config.listen_ip, global_config.listen_port);
    return true;
}

void NetManager::InitLogger()
{
    net_logger = spdlog::hourly_logger_mt<spdlog::async_factory>("net_logger", "log/net.log");
}

void NetManager::InitThreads(const std::string &listen_ip, uint16_t listen_port)
{
    m_io_threads.push_back(new ListenerThread);
    m_listener_thread = dynamic_cast<ListenerThread *>(m_io_threads[0]);
    m_listener_thread->Init(0, listen_ip, listen_port, 
        std::bind(&NetEventInterface::CreateNetPacket, m_net_event_handler),
        std::bind(&NetManager::AcceptIOEvent, this, std::placeholders::_1, 0));
    
    IOThread *io_thread = nullptr;
    for (uint16_t i = 1; i < global_config.io_thread_num; i++)
    {
        io_thread = new IOThread;
        m_io_threads.push_back(io_thread);
        m_io_threads.back()->Init(i, std::bind(&NetEventInterface::CreateNetPacket, m_net_event_handler),
            std::bind(&NetManager::AcceptIOEvent, this, std::placeholders::_1, i));    
    }

    for (auto &thread : m_io_threads)
    {
        thread->Start();
    }
}

void NetManager::Stop()
{
    CloseAllConnections();
    for (auto &thread : m_io_threads)
    {
        thread->Stop();
        thread->Join();
    }
    m_keep_alive = false;
    LOGI("io thread stoped");
}

uint32_t NetManager::Update()
{
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
            { LOGE("unkown event type{}", (int16_t)event->event_type); }
            break;
        }
        delete event;//由io_event_pipe创建
        ++handle_count;
        if (handle_count >= global_config.net_manager_handle_io_event_num_of_one_update) break;
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
    m_net_event_handler->OnNewConnection(connection->fd, connection->remote_ip, connection->remote_port);
}

void NetManager::OnRead(const ReadEvent &event)
{
    m_net_event_handler->OnReceivePacket(*event.packet);
}

void NetManager::OnCloseConnectionComplete(const CloseConnectionCompleteEvent &event)
{
    m_connection_manager.DisconnectFrom(event.connection_fd);
    m_net_event_handler->OnDisconnect(event.connection_fd);
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

bool NetManager::Send(int32_t connection_fd, const char *data_bytes, uint32_t data_len)
{
    if (data_len == 0)
    {
        LOGW("fd: {} send data length is 0", connection_fd);
        return false;
    }
    if (m_connection_manager.GetConnection(connection_fd) == nullptr)
    {
        LOGE("invalid connection_fd: {}", connection_fd);
        return false;
    }
    WriteEvent *event = new WriteEvent;
    event->connection_fd = connection_fd;
    event->packet = new PacketToSend(connection_fd, data_len);
    memcpy(event->packet->packet_bytes, data_bytes, data_len);
    uint16_t io_thread_id = HashToIoThread(connection_fd);
    m_io_threads[io_thread_id]->AcceptIOEvent(event);
    return true;
}

void NetManager::CloseConnection(int32_t connection_fd)
{
    uint16_t io_thread_id = HashToIoThread(connection_fd);
    CloseConnectionRequestEvent *event = new CloseConnectionRequestEvent;
    event->connection_fd = connection_fd;
    m_io_threads[io_thread_id]->AcceptIOEvent(event);
}

void NetManager::AcceptIOEvent(IOEvent *event, uint16_t thread_id)
{
    m_events_queue.Enqueue(event, thread_id);
}

uint16_t NetManager::HashToIoThread(int32_t connection_fd)
{
    uint16_t thread_id = connection_fd % (int32_t)global_config.io_thread_num;
    return thread_id;
}

void NetManager::CloseAllConnections()
{
    for (auto &iter : m_connection_manager.GetAllConnections())
    {
        CloseConnection(iter.first);
    }
}