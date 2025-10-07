#include <iostream>
#include <semaphore>
#include <thread>

int main()
{
	std::binary_semaphore event{ 0 };
	std::jthread worker{ [&event] {
		std::cout << "Worker: waiting for event...\n";
		event.acquire();
		std::cout << "Worker: event received!\n";
	} };

	std::this_thread::sleep_for(std::chrono::seconds(2));
	std::cout << "Main: signaling event.\n";
	event.release();
	worker.join();
}