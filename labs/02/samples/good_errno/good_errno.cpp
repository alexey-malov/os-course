#include <chrono>
#include <iostream>
#include <syncstream> // std::osyncstream
#include <thread> // std::jthread
#include <vector>

thread_local int g_lastError = 0; // У КАЖДОГО потока — СВОЯ копия

void Worker(int id, int work_ms)
{
	g_lastError = id;
	std::this_thread::sleep_for(std::chrono::milliseconds(work_ms));

	std::osyncstream(std::cout) << "[good] thread " << id
								<< " sees g_lastError=" << g_lastError << '\n';
}

int main()
{
	std::vector<std::jthread> ts;
	for (int i = 1; i <= 4; ++i)
	{
		ts.emplace_back(Worker, i, 50 * i);
	}
	// jthread сам делает join() в деструкторе
}
