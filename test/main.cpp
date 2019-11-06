#include <iostream> 
#include <sys/epoll.h>
#include <time.h>
#include <chrono>
#include <set>
#include <vector>
#include "../src/net_manager.h"


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

std::set<int32_t> connected_fd;
class NetHandler : public NetEventInterface
{
public:
    virtual NetPacketInterface *CreateNetPacket() override
	{
		return new NetPacket(512);
	}
	virtual void OnNewConnection(int32_t connection_fd, const std::string &ip, uint16_t port) override
	{
		std::cout << "new connection, ip[" << ip << "] port[" << port << "]" << std::endl;
		connected_fd.emplace(connection_fd);
	}
    virtual void OnReceivePacket(const NetPacketInterface &) override
	{

	}
    virtual void OnDisconnect(int32_t connection_fd) override
	{
		std::cout << "connection: " << connection_fd << " disconnect" << std::endl;
		connected_fd.erase(connection_fd);
	}
};

NetHandler handler;
NetManager net;

uint32_t total_send_num = 0;
int32_t SendMessage()
{
	static uint32_t sended_num = 0;
	char data[1024];
	for (int32_t fd : connected_fd)
	{
		net.Send(fd, data, 1024);
		++sended_num;
	}
	if (sended_num > total_send_num)
	{
		return 0;
	}
	return -1;
}

int main(int argc, char *argv[])
{
	NetConfig config;
	config.io_thread_num = std::atoi(argv[1]);
	config.listen_ip = "0.0.0.0";
	config.listen_port = 18889;
	net.Init(config, &handler);
	uint32_t handle_count = 0;
	uint32_t handleed_count = 0;
	auto start = std::chrono::system_clock::now();
	auto end = start;
	uint32_t total_handle_num = std::atoi(argv[2]);
	total_send_num = std::atoi(argv[3]);
	while (net.IsAlive() || handle_count != 0)
	{
		if (SendMessage() == 0)
		{
			net.Stop();
		    end = std::chrono::system_clock::now();
			break;
		}
		handle_count = net.Update();
		handleed_count += handle_count;
		if (handleed_count >= total_handle_num)
		{
		    net.Stop();
		    end = std::chrono::system_clock::now();
		}
		if (handle_count == 0)
		{
			WaitAWhile();
		}
	}
	do {
		handle_count = net.Update();
	} while(handle_count != 0);
	std::cout << "cost time:" << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << std::endl;
}
