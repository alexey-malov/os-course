#include <atomic>
#include <iostream>
#include <thread>
#include <mutex>


class TSLLock
{
public:
	TSLLock() = default;

	TSLLock(const TSLLock&) = delete;
	TSLLock& operator=(const TSLLock&) = delete;

	void lock()
	{
		// Крутимся в цикле, пока флаг не станет свободен
		// test_and_set возвращает старое значение флага и устанавливает его в true
		while (m_locked.test_and_set(std::memory_order_acquire))
			; // spin
	}

	void unlock()
	{
		// Сбрасываем флаг в false
		m_locked.clear(std::memory_order_release);
	}

private:
	std::atomic_flag m_locked = ATOMIC_FLAG_INIT;
};

int main()
{
	TSLLock lock;
	int counter = 0;
	std::jthread t1{ [&lock, &counter] {
		for (int i = 0; i < 10'000'000; ++i)
		{
			lock.lock();
			++counter;
			lock.unlock();
		}
	} };
	std::jthread t2{ [&lock, &counter] {
		for (int i = 0; i < 10'000'000; ++i)
		{
			// Конструктор lock_guard вызовет lock(), а деструктор - unlock()
			std::lock_guard lk{ lock };
			--counter; // Критическая секция
		}
	} };
	t1.join();
	t2.join();
	std::cout << "Counter: " << counter << '\n';
	return 0;
}