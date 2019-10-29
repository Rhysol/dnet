#pragma once
#include "io_event.h"
#include "mpsc_queue.h"
#include <unordered_map>
#include <deque>


class WriteHandler
{
public:
    WriteHandler();
    ~WriteHandler();

    void Init(const OutputIOEventPipe &pipe);

    void Send(PacketToSend *packet);

    bool HandleUnfinishedPacket(int32_t connection_fd);

    void OnCloseConnection(int32_t connection_fd);

private:
    void OnUnfinishedPacket(PacketToSend *packet);
    void SendUnfinishedPacket(PacketToSend *packet);
    void ClearUnfinishedPacket(int32_t connection_fd);

    void OnUnexpectedDisconnect(int32_t connection_fd);
    void OnWriteEagain(int32_t connection_fd);
private:
    OutputIOEventPipe m_output_io_event_pipe;

    std::unordered_map<int32_t, std::deque<PacketToSend *>> m_unfinished_packet;
};