#include <iostream>
#include <process.h>
#include <windows.h>

unsigned __stdcall Worker(void* arg)
{
	int id = *static_cast<int*>(arg);
	std::cout << "hello from thread " << id << "\n";
	return 0; // или: _endthreadex(0);
}

int main()
{
	int id = 1;
	unsigned tid = 0;
	HANDLE h = (HANDLE)_beginthreadex(nullptr, 0, &Worker, &id, 0, &tid);
	if (!h)
	{
		std::cerr << "_beginthreadex failed\n";
		return 1;
	}
	WaitForSingleObject(h, INFINITE);
	CloseHandle(h); // важно: _endthreadex хэндл НЕ закрывает
	return 0;
}
