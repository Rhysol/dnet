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

NetConfig dnet::global_config;
std::shared_ptr<spdlog::logger> dnet::net_logger;

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
}

bool NetManager::Init(const NetConfig &config, NetEventInterface *net_hander)
{
    if (net_hander == nullptr) return false;
    InitLogger();
    m_net_config = config;
    m_net_event_handler = net_hander;
    m_events_queue.Init(m_net_config.io_thread_num),
    InitThreads();
    return true;
}

void NetManager::InitLogger()
{
    net_logger = spdlog::hourly_logger_mt<spdlog::async_factory>("net_logger", "log/net.log");
}

void NetManager::InitThreads()
{
    m_io_threads.push_back(new ListenerThread);
    m_listener_thread = dynamic_cast<ListenerThread *>(m_io_threads[0]);
    m_listener_thread->Init(0, std::bind(&NetEventInterface::CreateNetPacket, m_net_event_handler), &m_net_config);
    m_listener_thread->SetNextPasser(this, IOEventPasser::EDestination::MAIN_THREAD);
    
    IOThread *io_thread = nullptr;
    for (uint16_t i = 1; i < m_net_config.io_thread_num; i++)
    {
        io_thread = new IOThread;
        m_io_threads.push_back(io_thread);
        io_thread->Init(i, std::bind(&NetEventInterface::CreateNetPacket, m_net_event_handler), &m_net_config);
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
        if (handle_count >= m_net_config.net_manager_handle_io_event_num_of_one_update) break;
        event = m_events_queue.Dequeue();
    }
    return handle_count;
}

//从listener thread接收的新连接
void NetManager::OnAcceptConnection(const AcceptConnectionEvent &event)
{
    RegisterConnectionEvent *register_event = new RegisterConnectionEvent;
    register_event->connection_fd = event.connection_fd; 
    Pass2IOThread(register_event, HashToIoThread(event.connection_fd));
    m_net_event_handler->OnNewConnection(event.connection_fd, event.remote_ip, event.remote_port);
}

void NetManager::OnRead(const ReadEvent &event)
{
    m_net_event_handler->OnReceivePacket(*event.packet);
}

void NetManager::OnCloseConnectionComplete(const CloseConnectionCompleteEvent &event)
{
    m_net_event_handler->OnDisconnect(event.connection_fd);
}

int32_t NetManager::ConnectTo(const std::string &remote_ip, uint16_t remote_port)
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
    if (connect(fd, (sockaddr *)&remote_addr, sizeof(sockaddr)) == -1)
    {
        LOGE("connect to: {}:{} failed! errno: {}", remote_ip, remote_port, errno);
        return -1;
    }

    if (fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK) == -1)
    {
        LOGE("set to nonblock failed! errno: {}", errno);
        return -1;
    }
    RegisterConnectionEvent *event = new RegisterConnectionEvent;
    event->connection_fd = fd; 
    Pass2IOThread(event, HashToIoThread(fd));
    return fd;
}

bool NetManager::Send(int32_t connection_fd, const char *data_bytes, uint32_t data_len)
{
    if (data_len == 0)
    {
        LOGW("fd: {} send data length is 0", connection_fd);
        return false;
    }
    WriteEvent *event = new WriteEvent;
    event->connection_fd = connection_fd;
    event->packet = new PacketToSend(connection_fd, data_len);
    memcpy(event->packet->packet_bytes, data_bytes, data_len);
    Pass2IOThread(event, HashToIoThread(connection_fd));
    return true;
}

void NetManager::CloseConnection(int32_t connection_fd)
{
    CloseConnectionRequestEvent *event = new CloseConnectionRequestEvent;
    event->connection_fd = connection_fd;
    Pass2IOThread(event, HashToIoThread(connection_fd));
}

uint16_t NetManager::HashToIoThread(int32_t connection_fd)
{
    uint16_t thread_id = connection_fd % (int32_t)m_net_config.io_thread_num;
    return thread_id;
}

void NetManager::Pass2MainThread(IOEvent *event)
{
    m_events_queue.Enqueue(event, event->source_thread_id);
}

void NetManager::Pass2IOThread(IOEvent *event, uint16_t io_thread_id)
{
    m_io_threads[io_thread_id]->Pass2IOThread(event);
}
