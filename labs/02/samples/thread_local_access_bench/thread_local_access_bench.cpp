#define CATCH_CONFIG_MAIN
#define CATCH_CONFIG_ENABLE_BENCHMARKING
#include <catch_amalgamated.hpp>
#include <thread>

// Глобальная переменная
volatile int g_var = 0;

// Thread-local переменная
thread_local volatile int g_tlVar = 0;

constexpr int N = 10'000'000;

void UseGlobalVariable()
{
	for (int i = 0; i < N; ++i)
	{
		++g_var;
	}
}

void UseThreadLocalVariable()
{
	for (int i = 0; i < N; ++i)
	{
		++g_tlVar;
	}
}

TEST_CASE("Global vs Thread-local access benchmark", "[benchmark]")
{
	BENCHMARK("Increment global variable")
	{
		UseGlobalVariable();
		return g_var;
	};

	BENCHMARK("Increment thread_local variable")
	{
		UseThreadLocalVariable();
		return g_tlVar;
	};
}
