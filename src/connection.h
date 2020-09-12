#pragma once
#include "io_event.h"
#include <unordered_map>
#include <deque>
#include "net_config.h"
#include <cstring>

namespace dnet
{

struct ReadBuffer
{
    ReadBuffer() {
    }
    ~ReadBuffer() {
    }
    inline static constexpr uint32_t Capacity() {
        return 65535;
    }
    char buffer[65535];
    uint32_t remain_len = 0;
    uint32_t offset = 0;
};

class Connection : public IOEventPasser
{
public:
    Connection();
    ~Connection();

    typedef std::function<NetPacketInterface *()> CreateNetPacketFunc;
    void Init(uint64_t id, int32_t fd, const NetConfig *net_config);
    inline bool HasInited() { return m_has_inited; }

    void Receive();

    void Send(std::vector<char> &packet);
    void SendRemainPacket();
    inline uint64_t GetID() { return m_id; }
    inline int32_t GetFD() { return m_fd; }

    inline bool Connected() { return m_connected; }
    void SetConnected(bool is_connected) { m_connected = is_connected; }

    //������д�Ĺ����з������ӶϿ��������������Ͽ�����
    void OnUnexpectedDisconnect();
private:
    void ParseReadBuffer();
    std::vector<char> &GetInCompletePacketBytes();

    void DoSendRemainPacket();

private:
    uint64_t m_id = 0;
    int32_t m_fd = -1;
    bool m_has_inited = false;
    bool m_connected = false;
    bool m_to_close = false;
    const NetConfig *m_net_config = nullptr;

    thread_local static ReadBuffer m_read_buffer;
    //ReadBuffer m_read_buffer;
    io_event::ReceiveAPacket *m_incomplete_receive = nullptr;
    uint32_t m_received_len = 0;

    bool m_can_send = true;
    uint32_t m_send_offset = 0;
    std::deque<std::vector<char>> m_packet_to_send;
};

}