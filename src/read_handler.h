#pragma once
#include "io_event.h"
#include <unordered_map>


struct ReadBuffer
{
    inline static uint32_t BufferMaxLen() {
        static uint32_t buffer_max_len = 65535;
        return buffer_max_len;
    }
    char buffer[65535];
    uint32_t buffer_len = 0;
};

class ReadHandler
{
public:
    ReadHandler();
    ~ReadHandler();

    typedef std::function<NetPacketInterface *()> CreateNetPacketFunc;
    void Init(const CreateNetPacketFunc &create_packt_func, const OutputIOEventPipe &pipe);

    void OnRead(int32_t connection_fd);

    //服务器主动断开连接时调用
    void OnCloseConnection(int32_t connection_fd);

private:
    void ParseReadBuffer(int32_t connection_fd);
    ReadEvent *GetUnfinishedReadEvent(int32_t connection_fd);
    ReadEvent *CreateReadEvent(int32_t connection_fd);
    void ClearUnfinishedRead(int32_t connection_fd);

    //读的过程中发现连接断开，服务器被动断开连接
    void OnUnexpectedDisconnect(int32_t connection_fd);

private:
    OutputIOEventPipe m_output_io_event_pipe;
    CreateNetPacketFunc m_create_net_packet_func;

    ReadBuffer m_read_buffer;
    std::unordered_map<int32_t, ReadEvent *> m_unfinished_read; //key:connection_fd
};