#include <csignal>

namespace
{
volatile sig_atomic_t stop_requested = 0;

void HandleSigterm(int)
{
	stop_requested = 1;
}
} // namespace

int main()
{
	struct sigaction sa{};
	sa.sa_handler = HandleSigterm;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	sigaction(SIGTERM, &sa, nullptr);

	while (!stop_requested)
	{
		// основная работа
	}

	// корректное завершение
}
