#include "lib.h"
#include <iostream>
#include <thread>

int main()
{
	int counter = 0;
	PetersonLock lock;
	std::jthread t1{ [&counter, &lock] {
		for (int i = 0; i < 10'000'000; ++i)
			Increment(lock, counter);
	} };
	std::jthread t2{ [&counter, &lock] {
		for (int i = 0; i < 10'000'000; ++i)
			Decrement(lock, counter);
	} };
	t1.join();
	t2.join();
	std::cout << "Counter: " << counter << '\n';
}
