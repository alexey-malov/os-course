#include <atomic>
#include <deque>
#include <iostream>
#include <limits>
#include <mutex>
#include <semaphore>
#include <thread>

int main()
{
	std::mutex m;
	int counter = 0;
	std::jthread t1{ [&] {
		for (int i = 0; i < 1'000'000; ++i)
		{
			std::lock_guard lock{ m };
			++counter;
		} } };

	std::jthread t2{ [&] {
		for (int i = 0; i < 1'000'000; ++i)
		{
			std::lock_guard lock{ m };
			--counter;
		} } };

	t1.join();	
	t2.join();
	std::cout << counter << std::endl;
}
