#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <latch>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <time.h>
#include <vector>

namespace
{

using Clock = std::chrono::steady_clock;

struct Options
{
	unsigned threads = 0;
	std::uint64_t iterations = 0;
	std::uint64_t durationMs = 0;
};

std::uint64_t ParseUInt(std::string_view text)
{
	std::uint64_t result = 0;
	const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), result);
	if (error != std::errc{} || end != text.data() + text.size())
	{
		throw std::invalid_argument("Invalid nonnegative integer: " + std::string(text));
	}
	return result;
}

void PrintUsage()
{
	std::cout << "CPU-local workload: independent CPU-bound threads\n"
			  << "Usage:\n"
			  << "  cpu-local --threads N --iterations TOTAL\n"
			  << "  cpu-local --threads N --duration-ms MS\n"
			  << "TOTAL is the total number of iterations, not iterations per thread.\n";
}

Options ParseArgs(int argc, char** argv)
{
	Options o;
	for (int i = 1; i < argc; ++i)
	{
		std::string_view name = argv[i];
		if (name == "--help")
		{
			PrintUsage();
			std::exit(0);
		}
		if (i + 1 >= argc)
		{
			throw std::invalid_argument("Missing value after " + std::string(name));
		}
		const auto value = ParseUInt(argv[++i]);
		if (name == "--threads")
		{
			if (value < 1 || value > 256)
			{
				throw std::invalid_argument("--threads must be between 1 and 256");
			}
			o.threads = static_cast<unsigned>(value);
		}
		else if (name == "--iterations")
		{
			o.iterations = value;
		}
		else if (name == "--duration-ms")
		{
			o.durationMs = value;
		}
		else
		{
			throw std::invalid_argument("Unknown option: " + std::string(name));
		}
	}
	if (o.threads == 0 || ((o.iterations > 0) == (o.durationMs > 0)))
	{
		throw std::invalid_argument("Specify --threads and exactly one of --iterations / --duration-ms");
	}
	return o;
}

std::int64_t ProcessCpuNs()
{
	::timespec ts{};
	if (::clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts) != 0)
	{
		throw std::runtime_error("clock_gettime(CLOCK_PROCESS_CPUTIME_ID) failed");
	}
	return static_cast<std::int64_t>(ts.tv_sec) * 1'000'000'000LL + ts.tv_nsec;
}

int Run(const Options& o)
{
	std::latch ready(static_cast<std::ptrdiff_t>(o.threads));
	std::latch start(1);
	std::vector<std::uint64_t> completed(o.threads, 0);
	std::vector<std::jthread> workers;
	workers.reserve(o.threads);

	// Главный поток задаёт время завершения перед началом работы.
	Clock::time_point deadline{};
	const std::uint64_t base = o.iterations / o.threads;
	const std::uint64_t extra = o.iterations % o.threads;

	for (unsigned index = 0; index < o.threads; ++index)
	{
		workers.emplace_back([&, index] {
			// У каждого потока своя volatile-переменная: общего счётчика нет.
			// volatile не позволяет оптимизатору удалить обращения к переменной,
			// но НЕ служит механизмом межпоточной синхронизации.
			volatile std::uint64_t local = 0;
			ready.count_down();
			start.wait();

			if (o.iterations != 0)
			{
				const std::uint64_t count = base + (index < extra ? 1 : 0);
				for (std::uint64_t j = 0; j < count; ++j)
				{
					const std::uint64_t current = local;
					local = current + 1;
				}
			}
			else
			{
				// Проверяем время только после блока вычислений.
				// Режим duration нужен как фон для конкуренции за CPU.
				while (Clock::now() < deadline)
				{
					for (unsigned j = 0; j < 16'384; ++j)
					{
						const std::uint64_t current = local;
						local = current + 1;
					}
				}
			}
			completed[index] = local;
		});
	}

	ready.wait();
	const auto begin = Clock::now();
	const auto cpuBegin = ProcessCpuNs();
	if (o.durationMs != 0)
	{
		deadline = begin + std::chrono::milliseconds(o.durationMs);
	}
	start.count_down();
	for (auto& worker : workers)
	{
		worker.join();
	}
	const auto end = Clock::now();
	const auto cpuEnd = ProcessCpuNs();

	std::uint64_t checksum = 0;
	for (auto value : completed)
	{
		checksum += value;
	}
	const double elapsedMs = std::chrono::duration<double, std::milli>(end - begin).count();
	const double processCpuMs = static_cast<double>(cpuEnd - cpuBegin) / 1'000'000.0;

	std::cout << std::fixed << std::setprecision(3)
			  << "program=cpu-local\n"
			  << "mode=" << (o.iterations ? "iterations" : "duration") << '\n'
			  << "threads=" << o.threads << '\n'
			  << "requested_iterations=" << o.iterations << '\n'
			  << "requested_duration_ms=" << o.durationMs << '\n'
			  << "elapsed_ms=" << elapsedMs << '\n'
			  << "process_cpu_ms=" << processCpuMs << '\n'
			  << "checksum=" << checksum << '\n';
	return 0;
}

} // namespace

int main(int argc, char** argv)
{
	try
	{
		return Run(ParseArgs(argc, argv));
	}
	catch (const std::exception& e)
	{
		std::cerr << "cpu-local: " << e.what() << '\n';
		PrintUsage();
		return 2;
	}
}
