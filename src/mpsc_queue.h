#pragma once

#include <atomic>
#include <cstdint>
#include <x86intrin.h>

namespace dnet
{

const static uint64_t MAX_UINT = -1;

template <typename T>
class MPSCQueue
{
	struct Node
	{
		T *item = nullptr;
		Node *next = nullptr;
		std::atomic<uint64_t> ts;

		Node() : ts(MAX_UINT) {}
	};
public:
	MPSCQueue() 
	{
	}

	~MPSCQueue()
	{
		Node *current = nullptr;
		Node *temp = nullptr;
		for (uint16_t i = 0; i < m_thread_num; i++)
		{
			current = m_heads[i];
			while (current != nullptr)
			{
				temp = current;
				current = current->next;
				delete temp;
			}
		}
		delete[] m_heads;
		delete[] m_tails;
	}

	void Init(uint16_t enqueue_thread_num)
	{
		m_thread_num = enqueue_thread_num;
		m_heads = new Node*[m_thread_num];
		m_tails = new Node*[m_thread_num];
		for (uint16_t i = 0; i < m_thread_num; i++)
		{
			m_heads[i] = new Node();
			m_tails[i] = m_heads[i];
		}
	}

	void Enqueue(T *item, uint16_t tid)
	{
		if (item == nullptr || tid >= m_thread_num) return;
		Node *last_tail = m_tails[tid];
		m_tails[tid] = new Node();
		last_tail->item = item;
		last_tail->next = m_tails[tid];
		last_tail->ts.store(__rdtsc(), std::memory_order_release);
	}

	T *Dequeue()
	{
		int16_t prev_tid = -2;
		uint64_t min_ts = 0;
		int16_t min_tid = 0;
		while (true)
		{
			min_ts = MAX_UINT;
			min_tid = -1;
			for (int16_t i = 0; i < (int16_t)m_thread_num; i++)
			{
				uint64_t ts = m_heads[i]->ts.load();
				if (ts < min_ts)
				{
					min_ts = ts;
					min_tid = i;
				}
			}
			if (min_tid == -1 && prev_tid == min_tid) return nullptr;
			if (prev_tid == min_tid)
			{
				Node *last_head = m_heads[min_tid];
				T *item = last_head->item;
				m_heads[min_tid] = last_head->next;
				delete last_head;
				return item;
			}
			prev_tid = min_tid;
		}
	}

private:
	std::atomic<uint64_t> m_current_ts;
	Node **m_heads = nullptr;
	Node **m_tails = nullptr;
	uint16_t m_thread_num = 0;
};

}