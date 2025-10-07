#include <atomic>
#include <deque>
#include <iostream>
#include <limits>
#include <mutex>
#include <semaphore>
#include <thread>

class Queue
{
public:
	explicit Queue(unsigned capacity)
		: m_emptySlots(capacity)
	{
	}

	Queue(const Queue&) = delete;
	Queue& operator=(const Queue&) = delete;

	void Push(int value)
	{
		m_emptySlots.acquire(); // Ждём свободное место
		{
			std::lock_guard lock{ m_mut };
			m_queue.push_back(value);
		}
		m_usedSlots.release(); // Уведомляем, что появился новый элемент
	}

	int Pop()
	{
		m_usedSlots.acquire(); // Ждем, пока появится элемент

		std::unique_lock lock{ m_mut };
		int value = m_queue.front();
		m_queue.pop_front();
		lock.unlock();
		
		m_emptySlots.release(); // Уведомляем, что освободилось место
		return value;
	}

private:
	std::counting_semaphore<> m_emptySlots;
	std::counting_semaphore<> m_usedSlots{ 0 };
	std::mutex m_mut;
	std::deque<int> m_queue;
};

int main()
{
	Queue q{ 10 };
	std::jthread consumer{ [&q] {
		for (int i = 0; i < 3; ++i)
		{
			int value = q.Pop();
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			std::cout << "Consuming: " << value << '\n';
		}
	} };
	std::jthread producer{ [&q] {
		for (int i = 0; i < 3; ++i)
		{
			std::cout << "Producing " << i << '\n';
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			q.Push(i);
		}
	} };
}