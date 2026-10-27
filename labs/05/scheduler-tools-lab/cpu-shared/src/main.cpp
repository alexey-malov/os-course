#include <atomic>
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
};

void PrintUsage()
{
	std::cout << "CPU-shared workload: contended atomic counter\n"
			  << "Usage: cpu-shared --threads N --iterations TOTAL\n"
			  << "TOTAL is split between all worker threads.\n";
}

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
		else
		{
			throw std::invalid_argument("Unknown option: " + std::string(name));
		}
	}
	if (o.threads == 0 || o.iterations == 0)
	{
		throw std::invalid_argument("Specify positive --threads and --iterations");
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
	// Все потоки намеренно изменяют одну общую атомарную переменную.
	std::atomic<std::uint64_t> counter{ 0 };
	std::latch ready(static_cast<std::ptrdiff_t>(o.threads));
	std::latch start(1);
	std::vector<std::jthread> workers;
	workers.reserve(o.threads);
	const auto base = o.iterations / o.threads;
	const auto extra = o.iterations % o.threads;

	for (unsigned index = 0; index < o.threads; ++index)
	{
		workers.emplace_back([&, index] {
			const std::uint64_t count = base + (index < extra ? 1 : 0);
			ready.count_down();
			start.wait();
			for (std::uint64_t j = 0; j < count; ++j)
			{
				// Достаточно атомарности: другие данные упорядочивать не нужно.
				counter.fetch_add(1, std::memory_order_relaxed);
			}
		});
	}

	ready.wait();
	const auto begin = Clock::now();
	const auto cpuBegin = ProcessCpuNs();
	start.count_down();
	for (auto& worker : workers)
	{
		worker.join();
	}
	const auto end = Clock::now();
	const auto cpuEnd = ProcessCpuNs();

	const double elapsedMs = std::chrono::duration<double, std::milli>(end - begin).count();
	const double processCpuMs = static_cast<double>(cpuEnd - cpuBegin) / 1'000'000.0;
	const auto checksum = counter.load(std::memory_order_relaxed);

	std::cout << std::fixed << std::setprecision(3)
			  << "program=cpu-shared\n"
			  << "threads=" << o.threads << '\n'
			  << "requested_iterations=" << o.iterations << '\n'
			  << "elapsed_ms=" << elapsedMs << '\n'
			  << "process_cpu_ms=" << processCpuMs << '\n'
			  << "checksum=" << checksum << '\n';
	if (checksum != o.iterations)
	{
		std::cerr << "ERROR: unexpected atomic checksum\n";
		return 1;
	}
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
		std::cerr << "cpu-shared: " << e.what() << '\n';
		PrintUsage();
		return 2;
	}
}
