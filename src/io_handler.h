#pragma once
#include "io_event.h"
#include <unordered_map>


struct epoll_event;

struct ReadBuffer
{
    inline static uint32_t BufferMaxLen() {
        static uint32_t buffer_max_len = 65535;
        return buffer_max_len;
    }
    char buffer[65535];
    uint32_t buffer_len = 0;
};

class IOHandler
{
public:
    IOHandler();
    ~IOHandler();

    typedef std::function<NetPacketInterface *()> CreateNetPacketFunc;
    typedef std::function<void (IOEvent *event)> OutputIOEventPipe;
    void Init(const CreateNetPacketFunc &create_packt_func, const OutputIOEventPipe &pipe);

    void HandleListenEvent(const epoll_event &ev, int32_t listener_fd);
    void HandleIOEvent(const epoll_event &ev);

private:
    void OnAccept(int32_t listener_fd);
    void OnDisconnect(int32_t connection_fd);

    void OnRead(int32_t connection_fd);
    void ParseReadBuffer(int32_t connection_fd);
    ReadEvent *GetUnfinishedReadEvent(int32_t connection_fd);
    ReadEvent *CreateReadEvent(int32_t connection_fd);

private:
    OutputIOEventPipe m_output_io_event_pipe;
    CreateNetPacketFunc m_create_net_packet_func;

    ReadBuffer m_read_buffer;
    std::unordered_map<int32_t, ReadEvent *> m_unfinished_read; //key:connection_fd
};