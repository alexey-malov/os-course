#include <chrono>
#include <iostream>
#include <syncstream> // std::osyncstream
#include <thread> // std::jthread
#include <vector>

int g_lastError = 0; // ОДНА глобальная переменная для всех потоков

void Worker(int id, int work_ms)
{
	g_lastError = id; // «Устанавливаем ошибку» для данного потока
	std::this_thread::sleep_for(std::chrono::milliseconds(work_ms));

	// osyncstream гарантирует, что строка выведется целиком,
	// не смешиваясь с выводом других потоков
	std::osyncstream(std::cout) << "[bad] thread " << id
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
