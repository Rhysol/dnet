#include "net_manager.h"
#include "listener_thread.h"
#include <cstring>
#include "net_config.h"
#include "logger.h"
#include <spdlog/sinks/hourly_file_sink.h>
#include <spdlog/async.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

using namespace dnet;

uint64_t dnet::g_dnet_time_ms = 0;

NetManager::NetManager()
{

}

NetManager::~NetManager()
{
    for (IOThread *thread : m_io_threads)
    {
        thread->Stop();
        thread->Join();
        delete thread;
    }
    delete m_net_config;
}

bool NetManager::Init(const NetConfig &config, NetEventInterface *net_hander)
{
    if (net_hander == nullptr) return false;
    g_dnet_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    m_net_event_handler = net_hander;
    m_net_config = new NetConfig(config);
    m_net_config->logger = spdlog::hourly_logger_mt<spdlog::async_factory>(m_net_config->logger_name, m_net_config->log_path);
    m_net_config->create_net_packet_func = std::bind(&NetEventInterface::CreateNetPacket, m_net_event_handler);
    m_events_queue.Init(m_net_config->io_thread_num),
    InitThreads();
    LOGI("start success");
    return true;
}

void NetManager::InitThreads()
{
    uint16_t i = 0;
    if (m_net_config->need_listener)
    {
        m_io_threads.push_back(new ListenerThread);
        m_listener_thread = dynamic_cast<ListenerThread *>(m_io_threads[0]);
        m_listener_thread->Init(0, m_net_config);
        m_listener_thread->SetNextPasser(this, IOEventPasser::EDestination::MAIN_THREAD);
        ++i;
    }
    IOThread *io_thread = nullptr;
    for (; i < m_net_config->io_thread_num; i++)
    {
        io_thread = new IOThread;
        m_io_threads.push_back(io_thread);
        io_thread->Init(i, m_net_config);
        io_thread->SetNextPasser(this, IOEventPasser::EDestination::MAIN_THREAD);
    }

    for (IOThread *thread : m_io_threads)
    {
        thread->Start();
    }
}

void NetManager::Stop()
{
    for (IOThread *thread : m_io_threads)
    {
        thread->Stop();
        thread->Join();
    }
    m_keep_alive = false;
    LOGI("io thread stoped");
}

uint32_t NetManager::Update()
{
    g_dnet_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    io_event::IOEvent *event = m_events_queue.Dequeue();
    uint16_t handle_count = 0;
    while (event != nullptr)
    {
        switch (event->event_type)
        {
        case io_event::EventType::ACCEPT_CONNECTION:
            OnAcceptConnection(*(io_event::AcceptConnection *)event);
            break;
        case io_event::EventType::RECEIVE_A_PACKET:
            OnReceiveAPacket(*(io_event::ReceiveAPacket *)event);
            break;
        case io_event::EventType::UNEXPECTED_DISCONNECT:
            OnDisconnect(event->connection_id);
            break;
        case io_event::EventType::NON_BLOCKING_CONNECT_RESULT:
            OnAsyncConnectResult(*(io_event::NonBlockingConnectResult *)event);
        default:
            { LOGE("unkown event type{}", (int16_t)event->event_type); }
            break;
        }
        delete event;//由io_event_pipe创建
        ++handle_count;
        if (handle_count >= m_net_config->net_manager_handle_io_event_num_of_one_update) break;
        event = m_events_queue.Dequeue();
    }
    return handle_count;
}

//从listener thread接收的新连接
void NetManager::OnAcceptConnection(const io_event::AcceptConnection &event)
{
    io_event::RegisterConnection *register_event = new io_event::RegisterConnection;
    register_event->connection_fd = event.connection_fd; 
    uint64_t connection_id = AllocateConnectionId();
    register_event->connection_id = connection_id;
    register_event->connected = true;
    Pass2IOThread(register_event);
    m_net_event_handler->OnAcceptConnection(connection_id, event.remote_ip, event.remote_port);
}

void NetManager::OnReceiveAPacket(const io_event::ReceiveAPacket &event)
{
    m_net_event_handler->OnReceivePacket(event.connection_id, *event.packet, event.source_thread_id);
}

void NetManager::OnDisconnect(uint64_t connection_id)
{
    m_net_event_handler->OnDisconnect(connection_id);
}

void NetManager::OnAsyncConnectResult(const io_event::NonBlockingConnectResult &event)
{
    m_net_event_handler->AsyncConnectResult(event.connection_id, event.is_success);
}

uint64_t NetManager::ConnectTo(const std::string &remote_ip, uint16_t remote_port, bool async)
{
    sockaddr_in remote_addr;
    remote_addr.sin_family = AF_INET;
    if (inet_aton(remote_ip.c_str(), &remote_addr.sin_addr) == 0)
    {
        LOGE("ip:{} is invalid!", remote_ip);
        return -1;
    }
    remote_addr.sin_port = htons(remote_port);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    uint64_t connection_id = -1;
    if (async ? NonBlockingConnect(fd, remote_addr, remote_ip, remote_port) : BlockingConnect(fd, remote_addr, remote_ip, remote_port))
    {
        connection_id = AllocateConnectionId();
        io_event::RegisterConnection *event = new io_event::RegisterConnection;
        event->connection_fd = fd; 
        event->connection_id = connection_id;
        event->connected = !async;
        Pass2IOThread(event);
    }
    return connection_id;
}

bool NetManager::BlockingConnect(int32_t connection_fd, sockaddr_in &remote_addr, const std::string &remote_ip, uint16_t remote_port)
{
    if (connect(connection_fd, (sockaddr *)&remote_addr, sizeof(sockaddr)) == -1)
    {
        LOGE("connect to: {}:{} failed! errno: {}", remote_ip, remote_port, errno);
        return false;
    }
    if (fcntl(connection_fd, F_SETFL, fcntl(connection_fd, F_GETFL, 0) | O_NONBLOCK) == -1)
    {
        LOGE("set to nonblock failed! errno: {}", errno);
        return false;
    }
    return true;
}

bool NetManager::NonBlockingConnect(int32_t connection_fd, sockaddr_in &remote_addr, const std::string &remote_ip, uint16_t remote_port)
{
    if (fcntl(connection_fd, F_SETFL, fcntl(connection_fd, F_GETFL, 0) | O_NONBLOCK) == -1)
    {
        LOGE("set to nonblock failed! errno: {}", errno);
        return false;
    }
    else if (connect(connection_fd, (sockaddr *)&remote_addr, sizeof(sockaddr)) == -1 && errno != EINPROGRESS)
    {
        LOGE("connect to: {}:{} failed! errno: {}", remote_ip, remote_port, errno);
        return false;
    }
    return true;
}

bool NetManager::Send(uint64_t connection_id, const char *data_bytes, uint32_t data_len)
{
    if (data_len == 0)
    {
        LOGW("connection_id: {} send data length is 0", connection_id);
        return false;
    }
    io_event::SendAPacket *event = new io_event::SendAPacket;
    event->connection_id = connection_id;
    event->packet = new PacketToSend(data_len);
    memcpy(event->packet->packet_bytes, data_bytes, data_len);
    Pass2IOThread(event);
    return true;
}

void NetManager::CloseConnection(uint64_t connection_id)
{
    io_event::IOEvent *event = new io_event::IOEvent(io_event::EventType::CLOSE_CONNECTION_REQUEST);
    event->connection_id = connection_id;
    Pass2IOThread(event);
}

uint16_t NetManager::HashToIoThread(uint64_t connection_id)
{
    uint16_t thread_id = connection_id % (int64_t)m_net_config->io_thread_num;
    return thread_id;
}

void NetManager::Pass2MainThread(io_event::IOEvent *event)
{
    m_events_queue.Enqueue(event, event->source_thread_id);
}

void NetManager::Pass2IOThread(io_event::IOEvent *event)
{
    uint16_t io_thread_id = HashToIoThread(event->connection_id);
    m_io_threads[io_thread_id]->Pass2IOThread(event);
}
