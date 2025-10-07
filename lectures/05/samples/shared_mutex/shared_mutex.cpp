#include <iostream>
#include <semaphore>
#include <shared_mutex>
#include <string>
#include <syncstream>
#include <thread>

class SafeString
{
public:
	std::string Read() const
	{
		std::shared_lock lock(m_mutex); // несколько читателей
		return m_data;
	}

	void Write(std::string s)
	{
		std::unique_lock lock(m_mutex); // один писатель
		m_data = std::move(s);
	}

private:
	mutable std::shared_mutex m_mutex;
	std::string m_data;
};

int main()
{
	SafeString str;
	std::jthread worker{ [&str] {
		for (int i = 0; i < 10; ++i)
		{
			std::string s = str.Read();
			std::osyncstream{ std::cout } << "Worker: read string: '" << s << "'\n";
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
		}
	} };

	for (int i = 0; i < 5; ++i)
	{
		std::string s = "Message " + std::to_string(i);
		str.Write(s);
		std::osyncstream{ std::cout } << "Main: wrote string: '" << s << "'\n";
		std::this_thread::sleep_for(std::chrono::milliseconds(800));
	}

	worker.join();
}