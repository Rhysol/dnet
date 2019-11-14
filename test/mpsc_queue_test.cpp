#include <iostream>
#include "../src/mpsc_queue.h"
#include <thread>
#include <vector>
#include <chrono>
#include <sstream>

struct Test
{
    int a;
    char b;
} obj;

uint16_t thread_num = 4;
uint32_t thread_enqueue_num = 250000;
uint32_t *thread_cost_time = nullptr;
MPSCQueue<Test> *queue = nullptr; 

void SetThreadNum(uint16_t num)
{
    thread_num = num;
}

void SetThreadEnqueueNum(uint32_t num)
{
    thread_enqueue_num = num;
}

void CreateQueue(uint16_t enqueue_thread_num)
{
    queue = new MPSCQueue<Test>;
    queue->Init(thread_num);
}

std::vector<std::thread> threads;

void DoEnqueue(uint16_t thread_id)
{
    auto start = std::chrono::system_clock::now();
    for (uint32_t i = 0; i < thread_enqueue_num; i++)
    {
        queue->Enqueue(&obj, thread_id);
    }
    auto end = std::chrono::system_clock::now();
    thread_cost_time[thread_id] = std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();
}

void StartEnqueueThreads()
{
    for(uint16_t i = 0; i < thread_num; i++)
    {
        threads.push_back(std::thread(DoEnqueue, i));
    }
}

void DoDequeue()
{
    uint32_t dequeue_num = thread_num * thread_enqueue_num;
    Test *p = nullptr;
    while (dequeue_num != 0)
    {
        if (queue->Dequeue() != nullptr)
        {
            --dequeue_num;
        }
    }
    
}

int main(int agrc, char *argv[])
{
    SetThreadNum(std::atoi(argv[1]));
    SetThreadEnqueueNum(std::atoi(argv[2]));
    thread_cost_time = new uint32_t[std::atoi(argv[1])];
    CreateQueue(thread_num);

    StartEnqueueThreads();
    auto start = std::chrono::system_clock::now();
    DoDequeue();
    auto end = std::chrono::system_clock::now();
    for (auto &thread : threads)
    {
        thread.join();
    }
    std::cout <<  "dequeue_num:" << thread_num * thread_enqueue_num << " cost:" << std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count() << std::endl;
    std::stringstream ss;
    uint32_t total = 0;
    for (uint16_t i = 0; i < thread_num; i++)
    {
        total += thread_cost_time[i];
        std::cout << "thread:" << i << " enqueue_num:" << thread_enqueue_num << " cost:" << thread_cost_time[i] << std::endl; 
    }
    std::cout << "enqueue average cost:" << total/thread_num << std::endl; 
}