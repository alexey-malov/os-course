#define CATCH_CONFIG_MAIN
#define CATCH_CONFIG_ENABLE_BENCHMARKING
#include <catch_amalgamated.hpp>
#include <latch>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

class FairRWLock
{
public:
	// Эксклюзивная блокировка (писатель)
	void lock()
	{
		std::unique_lock lk(m_mutex);
		++m_numWaitingWriters;
		m_writers.wait(lk, [&] {
			// Писатель может войти, если нет активных читателей и нет активного писателя
			return !m_isWriterActive && m_numActiveReaders == 0;
		});
		--m_numWaitingWriters; // Писатель больше не в очереди
		m_isWriterActive = true; // Писатель внутри
	}

	void unlock()
	{
		std::unique_lock lk(m_mutex);
		m_isWriterActive = false;

		// Если есть ожидающие писатели — будим одного из них
		if (m_numWaitingWriters > 0)
		{
			m_writers.notify_one();
		}
		else // иначе разрешаем всем читателям заходить.
		{
			m_readers.notify_all();
		}
	}

	// Совместная блокировка (читатель)
	void lock_shared()
	{
		std::unique_lock lk(m_mutex);
		m_readers.wait(lk, [&] {
			// Читатель может войти, если нет активного писателя и нет ожидающих писателей,
			// То есть, читатели не могут "проскочить" писателя
			return !m_isWriterActive && m_numWaitingWriters == 0;
		});
		++m_numActiveReaders;
	}

	void unlock_shared()
	{
		std::unique_lock lk(m_mutex);
		// Если мы здесь, значит внутри был хотя бы один читатель, а писатель — нет
		if (--m_numActiveReaders == 0)
		{
			if (m_numWaitingWriters > 0)
			{
				m_writers.notify_one();
			}
		}
	}

private:
	std::mutex m_mutex;
	std::condition_variable m_readers;
	std::condition_variable m_writers;

	int m_numActiveReaders = 0; // сколько читателей внутри
	int m_numWaitingWriters = 0; // сколько писателей ждёт
	bool m_isWriterActive = false; // есть ли писатель внутри?
};

namespace
{
template <typename SharedLockable>
int BenchLock(int numWriters, int numWriteIterations, int numReaders, int numReadIterations)
{
	SharedLockable lock;
	int counter = 0;
	std::vector<std::jthread> readers;

	// Защёлка чтобы читатели и писатели начали работать одновременно
	std::latch startLatch{ static_cast<std::ptrdiff_t>(numReaders + numWriters) };

	for (int i = 0; i < numReaders; ++i)
	{
		readers.emplace_back(
			[&] {
				startLatch.arrive_and_wait();
				for (int i = 0; ;)
				{
					std::shared_lock lk{ lock };
					if (counter == numWriteIterations && i == numReadIterations)
						break;
					if (i < numReadIterations)
						++i;
				} });
	}
	std::vector<std::jthread> writers;
	for (int i = 0; i < numWriters; ++i)
	{
		writers.emplace_back([&] {
				startLatch.arrive_and_wait();
				for (;;)
				{
					std::unique_lock lk{ lock };
					if (counter == numWriteIterations)
						break;
					++counter;
				} });
	}
	writers.clear(); // Ждем писателей
	readers.clear(); // Ждем читателей
	return counter;
}
} // namespace

TEST_CASE("FairRWLock vs std::shared_mutex")
{
	constexpr int NumIterations = 100'000;
	BENCHMARK("FairRWLock lock")
	{
		FairRWLock lock;
		int counter = 0;
		std::jthread t1{ [&] {
			for (int i = 0; i < NumIterations; ++i)
			{
				lock.lock();
				++counter;
				lock.unlock();
			} } };
		std::jthread t2{ [&] {
			for (int i = 0; i < NumIterations; ++i)
			{
				lock.lock();
				--counter;
				lock.unlock();
			} } };
		t1.join();
		t2.join();
		return counter;
	};
	BENCHMARK("shared_mutex lock")
	{
		std::shared_mutex lock;
		int counter = 0;
		std::jthread t1{ [&] {
			for (int i = 0; i < NumIterations; ++i)
			{
				lock.lock();
				++counter;
				lock.unlock();
			} } };
		std::jthread t2{ [&] {
			for (int i = 0; i < NumIterations; ++i)
			{
				lock.lock();
				--counter;
				lock.unlock();
			} } };
		t1.join();
		t2.join();
		return counter;
	};
	BENCHMARK("FairRWLock lock_shared")
	{
		FairRWLock lock;
		volatile int counter = 0;
		std::jthread t1{ [&] {
			for (int i = 0; i < NumIterations; ++i)
			{
				lock.lock_shared();
				lock.unlock_shared();
			} } };
		std::jthread t2{ [&] {
			for (int i = 0; i < NumIterations; ++i)
			{
				lock.lock_shared();
				lock.unlock_shared();
			} } };
		t1.join();
		t2.join();
		return counter;
	};
	BENCHMARK("shared_mutex lock_shared")
	{
		std::shared_mutex lock;
		volatile int counter = 0;
		std::jthread t1{ [&] {
			for (int i = 0; i < NumIterations; ++i)
			{
				lock.lock_shared();
				lock.unlock_shared();
			} } };
		std::jthread t2{ [&] {
			for (int i = 0; i < NumIterations; ++i)
			{
				lock.lock_shared();
				lock.unlock_shared();
			} } };
		t1.join();
		t2.join();
		return counter;
	};

	BENCHMARK("FairRWLock 2w*10k vs 10r*100k")
	{
		return BenchLock<FairRWLock>(2, 10'000, 10, 100'000);
	};

	BENCHMARK("std::shared_mutex 2w*10k vs 10r*100k")
	{
		return BenchLock<std::shared_mutex>(2, 10'000, 10, 100'000);
	};

	// Сценарий с 2 писателями и 10 читателями
	// (все писатели делают в сумме 10k операций, каждый читатель - по 10k)
	BENCHMARK("FairRWLock 2w*10k vs 10r*10k")
	{
		return BenchLock<FairRWLock>(2, 10'000, 10, 10'000);
	};

	BENCHMARK("std::shared_mutex 2w*10k vs 10r*10k")
	{
		return BenchLock<std::shared_mutex>(2, 10'000, 10, 10'000);
	};

	// Сценарий с 2 писателями и 10 читателями (писатели делают 100k операций, каждый чита
	BENCHMARK("FairRWLock 2w*100k vs 10r*10k")
	{
		return BenchLock<FairRWLock>(2, 100'000, 10, 10'000);
	};

	BENCHMARK("std::shared_mutex 2w*100k vs 10r*10k")
	{
		return BenchLock<std::shared_mutex>(2, 100'000, 10, 10'000);
	};
}