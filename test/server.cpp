#include <iostream> 
#include <sys/epoll.h>
#include <time.h>
#include <chrono>
#include <set>
#include <vector>
#include "../src/net_manager.h"
#include "../src/logger.h"
#include <signal.h>

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

NetManager net;

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
		LOGI("new connection, ip: {}:{}", ip, port);
		connected_fd.emplace(connection_fd);
	}
    virtual void OnReceivePacket(int32_t conncection_fd, const NetPacketInterface &packet) override
	{
		char *data = new char[packet.header_len + packet.body_len + 1];
		data[packet.header_len + packet.body_len] = '\0';
		memcpy(data, packet.header, packet.header_len);
		memcpy(data + packet.header_len, packet.body, packet.body_len);
		net.Send(conncection_fd, data, packet.header_len + packet.body_len);
		if (m_start_time == std::chrono::system_clock::time_point::min() && strncmp(data, "start", 5))
		{
			m_start_time = GetNowTime();
		}
		else if (m_end_time < GetNowTime() && strncmp(data, "end", 3))
		{
			m_end_time = GetNowTime();
		}
		delete[] data;
		++m_count;
	}
    virtual void OnDisconnect(int32_t connection_fd) override
	{
		LOGI("connection: {} disconnect", connection_fd);
		connected_fd.erase(connection_fd);
	}

    std::chrono::system_clock::time_point GetNowTime()
	{
		return std::chrono::system_clock::now();
	}

	void PrintCostTime()
	{
		auto duration = m_end_time - m_start_time;
		LOGI("handle {} cost {} ms", m_count, std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());
	}

private:
	std::chrono::system_clock::time_point m_start_time = std::chrono::system_clock::time_point::min();
	std::chrono::system_clock::time_point m_end_time = std::chrono::system_clock::time_point::min();
	uint32_t m_count = 0;
};

NetHandler handler;

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

void HandleStopSig(int sig)
{
	if(sig == SIGUSR1)
	{
		net.Stop();
	}
}

int main(int argc, char *argv[])
{

	NetConfig config;
	config.io_thread_num = std::atoi(argv[1]);
	config.listen_ip = "0.0.0.0";
	config.listen_port = 18889;
	net.Init(config, &handler);
	if (signal(SIGUSR1, &HandleStopSig) == SIG_ERR)
	{
		LOGE("register signal failed");
		return -1;
	}
	while (net.IsAlive())
	{
		if (!net.Update())
		{
			WaitAWhile();
		}
	}
	while(net.Update()) {}
	handler.PrintCostTime();
	return 0;
}
