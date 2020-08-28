#pragma once
#include "io_event.h"
#include <unordered_map>
#include <deque>
#include "net_config.h"

namespace dnet
{

struct ReadBuffer
{
    ReadBuffer() {
        buffer = new char[global_config.read_buffer_size];
    }
    ~ReadBuffer() {
        delete[] buffer;
    }
    inline static uint32_t BufferMaxLen() {
        return global_config.read_buffer_size;
    }
    char *buffer;
    int32_t buffer_len = 0;
};

class Connection : public IOEventPasser
{
public:
    Connection();
    ~Connection();

    typedef std::function<NetPacketInterface *()> CreateNetPacketFunc;
    void Init(int32_t fd, const NetConfig *net_config, CreateNetPacketFunc create_net_packet_func);


    void Read();

    void Write(PacketToSend *packet);
    //���������л�ѹ�İ�����true, ���򷵻�false
    bool HandleUnfinishedWrite();
    int32_t GetConnectionFD() { return m_fd; }

private:
    void ParseReadBuffer();
    ReadEvent *CreateReadEvent();

    void OnWriteUnfinished(PacketToSend *packet);
    void OnWriteEagain();

    //������д�Ĺ����з������ӶϿ��������������Ͽ�����
    void OnUnexpectedDisconnect();
private:
    int32_t m_fd;
    const NetConfig *m_net_config = nullptr;
    CreateNetPacketFunc m_create_net_packet_func;

    ReadBuffer m_read_buffer;
    ReadEvent *m_unfinished_read;

    std::deque<PacketToSend *> m_unfinished_write;
};

}