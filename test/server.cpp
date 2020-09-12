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

NetManager net;

std::set<uint64_t> connected_id;
class NetHandler : public NetEventHandler
{
public:
	void Init(uint32_t thread_num, NetConfig *config)
	{
		m_count.resize(thread_num);
		m_net_config = config;
	}
	virtual uint32_t GetBodyLenFromHeader(const char *) override
	{
		return 512;
	}
	virtual void OnAcceptConnection(uint64_t connection_id, const std::string &ip, uint16_t port) override
	{
		LOGI("new connection{}, ip: {}:{}", connection_id, ip, port);
		connected_id.emplace(connection_id);
	}
    virtual void OnReceivePacket(uint64_t conncection_id, std::vector<char> &packet, uint32_t thread_id) override
	{
		net.Send(conncection_id, packet.data(), packet.size());
		if (m_start_time == std::chrono::system_clock::time_point::min() && strncmp(packet.data(), "start", 5))
		{
			m_start_time = GetNowTime();
		}
		else if (m_end_time < GetNowTime() && strncmp(packet.data(), "end", 3))
		{
			m_end_time = GetNowTime();
		}
		++m_count[thread_id];
	}
    virtual void OnDisconnect(uint64_t connection_id) override
	{
		LOGI("connection: {} disconnect", connection_id);
		connected_id.erase(connection_id);
	}

    std::chrono::system_clock::time_point GetNowTime()
	{
		return std::chrono::system_clock::now();
	}

	void PrintCostTime()
	{
		auto duration = m_end_time - m_start_time;
		for (uint32_t i = 0; i < m_count.size(); i++)
		{
			LOGI("thread {} handle count : {}", i, m_count[i]);
		}
		LOGI("cost {} ms", std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());
	}

private:
	std::chrono::system_clock::time_point m_start_time = std::chrono::system_clock::time_point::min();
	std::chrono::system_clock::time_point m_end_time = std::chrono::system_clock::time_point::min();
	std::vector<uint32_t> m_count;
	NetConfig *m_net_config;
};

NetHandler handler;

void HandleStopSig(int sig)
{
	if(sig == SIGUSR1)
	{
		net.Stop();
	}
}

int main(int argc, char *argv[])
{
	if (argc < 1)
	{
		//LOGE("paramter num less than 1");
		return -1;
	}
	NetConfig config;
	config.io_thread_num = std::atoi(argv[1]);
	config.listen_ip = "0.0.0.0";
	config.listen_port = 18889;
	config.packet_header_len = 512;
	if (!net.Init(config, &handler))
	{
		return -1;
	}
	handler.Init(config.io_thread_num, net.GetConfig());
	if (signal(SIGUSR1, &HandleStopSig) == SIG_ERR)
	{
		//LOGE("register signal failed");
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
