#pragma once

#include <cstdint>

namespace game
{

// CPU-bound вычисления с последовательной зависимостью между итерациями.
// Внутри цикла нет ожиданий, ввода-вывода, атомиков и блокировок.
std::uint64_t Compute(std::uint64_t iterations, std::uint64_t seed);

// Измерение CPU-времени текущего потока через CLOCK_THREAD_CPUTIME_ID.
double ThreadCpuMilliseconds();

// Оцениваем, сколько итераций Compute() соответствует 1 мс CPU-времени.
// Это приближение, а не гарантированная оценка WCET.
std::uint64_t CalibrateIterationsPerMillisecond();

std::uint64_t IterationsForMilliseconds(double workMs, std::uint64_t iterationsPerMs);

} // namespace game
