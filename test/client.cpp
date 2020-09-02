#include "net_manager.h"
#include "logger.h"
#include <unordered_set>
#include <array>

using namespace dnet;


void WaitAWhile()
{
	timespec t;
	t.tv_sec = 0;
	t.tv_nsec = 1000 * 1000; //1ms
	nanosleep(&t, NULL);
}

class NetPacket : public NetPacketInterface
{
public:
	NetPacket(uint32_t header_len) : NetPacketInterface(header_len)
	{

	}
    uint32_t ParseBodyLenFromHeader() override {
		return 512;
	}
};

std::unordered_set<int32_t> connected_fd;
class NetHandler : public NetEventInterface
{
public:
    virtual NetPacketInterface *CreateNetPacket() override
	{
		return new NetPacket(512);
	}
	virtual void OnNewConnection(int32_t connection_fd, const std::string &ip, uint16_t port) override
	{
		LOGI("new connection, ip: {}:{}", ip, port);
		connected_fd.emplace(connection_fd);
	}
    virtual void OnReceivePacket(int32_t connection_fd, const NetPacketInterface &) override
	{
		++m_count;
	}
    virtual void OnDisconnect(int32_t connection_fd) override
	{
		LOGI("handle {} connection: {} disconnect", m_count, connection_fd);
		connected_fd.erase(connection_fd);
	}

	uint32_t m_count = 0;
};

NetManager net;
NetHandler handler;

int main(int argc, char *argv[])
{
	NetConfig config;
	config.io_thread_num = 1;
	config.need_listener = false;
	net.Init(config, &handler);
	int32_t connection_fd = net.ConnectTo("127.0.0.1", 18889);
	if(connection_fd <= 0)
	{
		LOGE("connect failed!");
	}
	std::array<char, 512> send_data = {'h', 'e', 'l', 'l', 'o', 'w', 'o', 'r', 'l', 'd'};
	uint32_t total_send_num = std::atoi(argv[1]);
	for (uint32_t i = 0; i < total_send_num; i++)
	{
		net.Update();
		net.Send(connection_fd, send_data.data(), send_data.max_size());
		WaitAWhile();
	}
	net.Stop();

	LOGI("success count: {}", handler.m_count);
}

