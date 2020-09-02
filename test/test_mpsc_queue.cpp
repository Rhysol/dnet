#include "mpsc_queue.h"
#include <iostream>
#include <atomic>
#include <thread>
#include <signal.h>
#include <time.h>
#include <array>


using namespace dnet;

struct MyStruct
{
    int i = 1;
    char b = 'c';
};

void WaitAWhile()
{
	timespec t;
	t.tv_sec = 0;
	t.tv_nsec = 1000 * 1000; //1ms
	nanosleep(&t, NULL);
}

constexpr int producer_num = 5;
std::array<std::thread *, producer_num> producer_thread;
std::array<std::atomic<bool>, producer_num> producer_switch;
std::atomic<bool> consumer_switch;

MPSCQueue<MyStruct> queue;

void Produce(int thread_id)
{
	int i = 0;
	while (producer_switch[thread_id].load())
	{
		queue.Enqueue(new MyStruct, thread_id);
		if ((i++) % 500 == 0)
		{
			WaitAWhile();
		}
	}
}

void handle_sig(int sig)
{
    if (sig == SIGUSR1)
    {
        for (int i = 0; i < producer_num; i++)
        {
            producer_switch[i].store(false);
		}
		consumer_switch.store(false);
    }
}

int main()
{
    if (signal(SIGUSR1, &handle_sig) == SIG_ERR)
    {
        std::cout << "register sig failed" << std::endl;
        return -1;
    }
	queue.Init(producer_num);
	for (int i = 0; i < producer_num; i++)
	{
		producer_switch[i].store(true);
		producer_thread[i] = new std::thread(&Produce, i);
	}
	consumer_switch.store(true);

	int i = 0;
	MyStruct *tmp = nullptr;
	while(consumer_switch.load())
	{
		tmp = queue.Dequeue();
		if (tmp)
		{
			delete tmp;
			++i;
		}
	}

	for (int i = 0; i < producer_num; i++)
	{
		producer_thread[i]->join();
		delete producer_thread[i];
	}

	while(tmp = queue.Dequeue())
	{
		delete tmp;
		++i;
	}
	std::cout << "finish: " << i << std::endl;
}
