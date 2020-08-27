#pragma once
#include <atomic>

namespace dnet
{
	
template <typename T>
class SPSCQueue
{
	struct Node
	{
		T *item = nullptr;
		Node *next = nullptr;
		std::atomic<bool> is_sentinel;

		Node() : is_sentinel(true) {}
	};
public:
	SPSCQueue() 
	{
		m_head = new Node;
		m_tail = m_head;
	}

	~SPSCQueue()
	{
		Node *temp = m_head;
        while (m_head != nullptr)
        {
            temp = m_head;
            m_head = m_head->next;
            delete temp;
        }
	}

	void Enqueue(T *item)
	{
		if (item == nullptr) return;
		Node *last_tail = m_tail;
		m_tail = new Node();
		last_tail->item = item;
		last_tail->next = m_tail;
		last_tail->is_sentinel.store(false, std::memory_order_release);
	}

	T *Dequeue()
	{
        if(m_head->is_sentinel.load()) return nullptr;

        Node *last_head = m_head;
        T *item = last_head->item;
        m_head = last_head->next;
        delete last_head;
        return item;
	}

private:
	Node *m_head = nullptr;
	Node *m_tail = nullptr;
};

}