#include <cstring>
#include <iostream>
#include <pthread.h>
#include <sched.h>
#include <vector>

void* Worker(void* arg)
{
	int id = *static_cast<int*>(arg);
	for (int k = 0; k < 3; ++k)
	{
		std::cout << "Worker " << id << " iteration " << k << "\n";
		sched_yield(); // в духе pthread_yield
	}
	return nullptr; // эквивалент pthread_exit(nullptr)
}

int main()
{
	constexpr int N = 4;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 1 << 20); // 1 МБ для примера

	std::vector<pthread_t> th(N);
	std::vector<int> ids(N);

	for (int i = 0; i < N; ++i)
	{
		ids[i] = i;
		if (int rc = pthread_create(&th[i], &attr, &Worker, &ids[i]); rc != 0)
		{
			std::cerr << "pthread_create: " << std::strerror(rc) << "\n";
			return 1;
		}
		std::cout << "main: started thread #" << ids[i] << "\n";
	}

	pthread_attr_destroy(&attr);

	for (int i = 0; i < N; ++i)
	{
		if (int rc = pthread_join(th[i], nullptr); rc != 0)
		{
			std::cerr << "pthread_join: " << std::strerror(rc) << "\n";
		}
	}
	std::cout << "main: all threads joined\n";
}
