#include "Workload.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <time.h>

namespace game
{

std::uint64_t Compute(std::uint64_t iterations, std::uint64_t seed)
{
	// Вычисления намеренно зависят от результата предыдущей итерации.
	// Результат сохраняется, чтобы компилятор не удалил вычисления.
	std::uint64_t value = seed | 1u;
	for (std::uint64_t i = 0; i < iterations; ++i)
	{
		value ^= value << 13;
		value ^= value >> 7;
		value ^= value << 17;
	}
	return value;
}

double ThreadCpuMilliseconds()
{
	timespec ts{};
	if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) != 0)
		throw std::runtime_error("clock_gettime(CLOCK_THREAD_CPUTIME_ID) failed");
	return static_cast<double>(ts.tv_sec) * 1000.0
		+ static_cast<double>(ts.tv_nsec) / 1'000'000.0;
}

std::uint64_t CalibrateIterationsPerMillisecond()
{
	std::uint64_t iterations = 100'000;
	double elapsedMs = 0.0;
	std::uint64_t result = 0x123456789ABCDEFULL;
	// Интервал примерно от 70 мс CPU нужен для более устойчивой калибровки.
	for (int pass = 0; pass < 12; ++pass)
	{
		const double begin = ThreadCpuMilliseconds();
		result ^= Compute(iterations, result + 0x9E3779B97F4A7C15ULL);
		elapsedMs = ThreadCpuMilliseconds() - begin;
		if (elapsedMs >= 70.0 || iterations >= 500'000'000ULL)
			break;
		iterations = std::min<std::uint64_t>(iterations * 2ULL, 500'000'000ULL);
	}
	// Не позволяем оптимизатору исключить калибровочный цикл.
	volatile std::uint64_t calibrationSink = result;
	(void)calibrationSink;
	if (!(elapsedMs > 0.001))
		throw std::runtime_error("CPU-work calibration failed");
	auto value = static_cast<std::uint64_t>(std::llround(iterations / elapsedMs));
	return std::max<std::uint64_t>(1, value);
}

std::uint64_t IterationsForMilliseconds(double workMs, std::uint64_t iterationsPerMs)
{
	if (!std::isfinite(workMs) || workMs <= 0)
		throw std::invalid_argument("work must be a positive finite number of milliseconds");
	const long double result = static_cast<long double>(workMs) * iterationsPerMs;
	if (result >= static_cast<long double>(std::numeric_limits<std::uint64_t>::max()))
		throw std::invalid_argument("work is too large");
	return std::max<std::uint64_t>(1, static_cast<std::uint64_t>(std::llround(result)));
}

} // namespace game
