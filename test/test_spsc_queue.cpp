#include "spsc_queue.h"
#include <signal.h>
#include <atomic>
#include <iostream>
#include <thread>
#include <time.h>
using namespace dnet;

void WaitAWhile()
{
	timespec t;
	t.tv_sec = 0;
	t.tv_nsec = 1000 * 1000; //1ms
	nanosleep(&t, NULL);
}

struct MyStruct
{
    int i = 1;
    char b = 'c';
};

SPSCQueue<MyStruct> queue;
std::atomic<bool> stop1;
std::atomic<bool> stop2;

void handle_sig(int sig)
{
    if (sig == SIGUSR1)
    {
        stop1.store(true);
        stop2.store(true);
    }
}

void output()
{
    MyStruct *tmp = nullptr;
    while(!stop2.load())
    {
        tmp = queue.Dequeue();
        if (tmp)
        {
            delete tmp;
        }
    }
}

int main()
{
    if (signal(SIGUSR1, &handle_sig) == SIG_ERR)
    {
        std::cout << "register sig failed" << std::endl;
        return -1;
    }
    stop1.store(false);
    stop2.store(false);
    for (int i = 0; i < 100; i++)
    {
        queue.Enqueue(new MyStruct);
    }
    std::thread t(&output);
    int i = 0;
    while(!stop1.load())
    {
        queue.Enqueue(new MyStruct);
        if ((i++) % 500 == 0)
        {
            WaitAWhile();
        }
    }
    t.join();
    std::cout << "start dequeue all" << std::endl;
    MyStruct *tmp = nullptr;
    while((tmp = queue.Dequeue()))
    {
        delete tmp;
        ++i;
    }
    std::cout << "finished: " << i << std::endl;
    return 0;
}