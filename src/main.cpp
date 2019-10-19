#include <iostream> 
#include <sys/epoll.h>
#include "listener.h"
#include <time.h>
#include "net_manager.h"
#include <chrono>


void WaitAWhile()
{
	timespec t;
	t.tv_sec = 0;
	t.tv_nsec = 1000 * 1000; //1ms
	nanosleep(&t, NULL);
}

struct NetPacket : public NetPacketInterface
{
	NetPacket(uint32_t header_len) : NetPacketInterface(header_len)
	{

	}
    uint32_t ParseBodyLenFromHeader() override {
		return 512;
	}
};

NetPacketInterface *CreateNetPacket()
{
	return new NetPacket(512);
}

int main(int argc, char *argv[])
{
	NetManager net;
	net.Init(std::atoi(argv[1]), "0.0.0.0", 18889, std::bind(&CreateNetPacket), nullptr);
	uint32_t handle_count = 0;
	uint32_t handleed_count = 0;
	auto start = std::chrono::system_clock::now();
	auto end = start;
	uint32_t total_handle_num = std::atoi(argv[2]);
	while (net.IsAlive() || handle_count != 0)
	{
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
