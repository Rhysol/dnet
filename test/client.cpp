#include "net_manager.h"
#include "logger.h"
#include <unordered_set>
#include <array>
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
    uint32_t ParseBodyLenFromHeader(const char *) override {
		return 512;
	}
};

class NetHandler : public NetEventInterface
{
public:
	void Init(NetConfig *config)
	{
		m_net_config = config;
	}
    virtual NetPacketInterface *CreateNetPacket() override
	{
		return new NetPacket(512);
	}
	virtual void OnAcceptConnection(uint64_t, const std::string &, uint16_t) override
	{
	}
	virtual void OnReceivePacket(uint64_t, NetPacketInterface &, uint32_t) override
	{
		++m_count;
	}
    virtual void OnDisconnect(uint64_t connection_id) override
	{
		LOGI("connection: {} disconnect", connection_id);
	}

	uint32_t m_count = 0;
	NetConfig *m_net_config = nullptr;
};

std::vector<std::atomic<bool> *> threads_switch;
void thread_func(uint32_t thread_id, uint32_t total_send_num)
{
    NetManager net;
    NetHandler handler;

	NetConfig config;
	config.io_thread_num = 1;
	config.need_listener = false;
	config.logger_name = "thread_logger_";
	config.logger_name.append(std::to_string(thread_id));
	config.log_path = "log/client_thread_";
	config.log_path.append(std::to_string(thread_id));
	config.log_path.append(".log");
	if (!net.Init(config, &handler)) return;
	handler.Init(net.GetConfig());
	NetConfig *m_net_config = net.GetConfig();
	uint64_t connection_id = net.ConnectTo("127.0.0.1", 18889);
	if(connection_id <= 0)
	{
		LOGE("connect failed!");
		return;
	}
	std::array<char, 1024> start_data = { 's', 't', 'a', 'r', 't' };
	std::array<char, 1024> send_data = {'h', 'e', 'l', 'l', 'o', 'w', 'o', 'r', 'l', 'd'};
	std::array<char, 1024> end_data = { 'e', 'n', 'd' };
	for (uint32_t i = 0; i < total_send_num; i++)
	{
		net.Update();
		if (0 == i)
		{
            net.Send(connection_id, start_data.data(), start_data.max_size());
		}
		else if (total_send_num - 1 == i)
		{
            net.Send(connection_id, end_data.data(), end_data.max_size());
		}
		else
		{
            net.Send(connection_id, send_data.data(), send_data.max_size());
		}
		WaitAWhile();
	}

	while(handler.m_count != total_send_num && threads_switch[thread_id]->load())
	{
		net.Update();
		WaitAWhile();
	}
	net.Stop();
	LOGI("thread: {} success count: {}", thread_id, handler.m_count);
}

void handle_sig(int sig)
{
	if (SIGUSR1 == sig)
	{
		for (auto thread_switch : threads_switch)
		{
			thread_switch->store(false);
		}
	}
}

int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		//LOGE("paramter num less than 1");
		return -1;
	}
	uint32_t thread_num = std::atoi(argv[1]);
	uint32_t total_send_num = std::atoi(argv[2]);
	if (signal(SIGUSR1, &handle_sig) == SIG_ERR)
	{
		//LOGE("register signal failed");
		return -1;
	}

	std::vector<std::thread *> threads;
	for(uint32_t i = 0; i < thread_num; i++)
	{
		threads.push_back(new std::thread(&thread_func, i, total_send_num));
		threads_switch.push_back(new std::atomic<bool>);
		threads_switch[i]->store(true);
	}
	for(uint32_t i = 0; i < thread_num; i++)
	{
		threads[i]->join();
	}
	return 0;
}

