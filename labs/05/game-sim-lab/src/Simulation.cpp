#include "Simulation.h"
#include "Workload.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <latch>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <sys/syscall.h>
#include <thread>
#include <time.h>
#include <unistd.h>

namespace game
{

namespace
{

int ParseInt(const std::string& value, const std::string& key)
{
	std::size_t used = 0;
	int result = 0;
	try
	{
		result = std::stoi(value, &used);
	}
	catch (...)
	{
		throw std::invalid_argument("invalid integer for " + key + ": " + value);
	}
	if (used != value.size())
		throw std::invalid_argument("invalid integer for " + key);
	return result;
}

double ParseDouble(const std::string& value, const std::string& key)
{
	std::size_t used = 0;
	double result = 0;
	try
	{
		result = std::stod(value, &used);
	}
	catch (...)
	{
		throw std::invalid_argument("invalid number for " + key + ": " + value);
	}
	if (used != value.size() || !std::isfinite(result))
		throw std::invalid_argument("invalid number for " + key);
	return result;
}

std::uint64_t ParseUInt64(const std::string& value, const std::string& key)
{
	if (value.empty() || value[0] == '-')
		throw std::invalid_argument("invalid unsigned integer for " + key);
	std::size_t used = 0;
	std::uint64_t result = 0;
	try
	{
		result = std::stoull(value, &used);
	}
	catch (...)
	{
		throw std::invalid_argument("invalid integer for " + key);
	}
	if (used != value.size())
		throw std::invalid_argument("invalid integer for " + key);
	return result;
}

std::vector<int> ParseCpuIndices(const std::string& value, const std::string& key)
{
	std::vector<int> indices;
	std::istringstream in(value);
	std::string part;
	while (std::getline(in, part, ','))
	{
		const int index = ParseInt(part, key);
		if (index < 0)
			throw std::invalid_argument(key + " requires nonnegative pool indices");
		indices.push_back(index);
	}
	if (indices.empty() || value.back() == ',')
		throw std::invalid_argument("invalid CPU-index list for " + key);
	std::sort(indices.begin(), indices.end());
	if (std::adjacent_find(indices.begin(), indices.end()) != indices.end())
		throw std::invalid_argument("duplicate index for " + key);
	return indices;
}

std::vector<int> GetAllowedCpus()
{
	cpu_set_t mask{};
	CPU_ZERO(&mask);
	if (sched_getaffinity(0, sizeof(mask), &mask) != 0)
		throw std::runtime_error(std::string("sched_getaffinity: ") + std::strerror(errno));
	std::vector<int> cpus;
	for (int id = 0; id < CPU_SETSIZE; ++id)
		if (CPU_ISSET(id, &mask))
			cpus.push_back(id);
	return cpus;
}

void ConstrainMainThreadToCpuPool(const std::vector<int>& selected)
{
	cpu_set_t mask{};
	CPU_ZERO(&mask);
	for (int id : selected)
		CPU_SET(id, &mask);
	// Созданные потоки наследуют эту маску; базовый сценарий работает
	// ещё до реализации студентами настройки affinity отдельных потоков.
	if (sched_setaffinity(0, sizeof(mask), &mask) != 0)
		throw std::runtime_error(std::string("sched_setaffinity(main): ") + std::strerror(errno));
}

Clock::time_point AtMs(const Clock::time_point& origin, double ms)
{
	return origin + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double, std::milli>(ms));
}

double SinceMs(Clock::time_point origin, Clock::time_point time)
{
	return std::chrono::duration<double, std::milli>(time - origin).count();
}

void RunPeriodic(WorkerData& output, Clock::time_point origin,
	const Config& c, double periodMs, double workMs,
	std::uint64_t iterationsPerMs,
	std::stop_token stop)
{
	const auto workIterations = IterationsForMilliseconds(workMs, iterationsPerMs);
	const double endMs = c.durationSeconds * 1000.0;
	std::uint64_t checksum = 0x9E3779B97F4A7C15ULL ^ static_cast<unsigned>(output.index + 42);
	for (std::uint64_t index = 0; index * periodMs < endMs - 0.000001; ++index)
	{
		if (stop.stop_requested())
			break;
		const double releaseMs = static_cast<double>(index) * periodMs;
		const auto release = AtMs(origin, releaseMs);
		const auto deadline = AtMs(origin, releaseMs + periodMs);
		std::this_thread::sleep_until(release);
		const auto started = Clock::now();
		Event event;
		event.index = index;
		event.releaseMs = releaseMs;
		// Если дедлайн уже прошёл, задание пропускается и учитывается
		// как нарушение, а не исчезает незаметно из статистики.
		if (started >= deadline)
		{
			event.skipped = true;
			event.missedDeadline = true;
			output.events.push_back(event);
			continue;
		}
		event.startMs = SinceMs(origin, started);
		const double cpuStart = ThreadCpuMilliseconds();
		checksum = Compute(workIterations, checksum + index + 1);
		const double cpuEnd = ThreadCpuMilliseconds();
		const auto finished = Clock::now();
		event.finishMs = SinceMs(origin, finished);
		event.cpuMs = cpuEnd - cpuStart;
		event.missedDeadline = finished > deadline;
		output.events.push_back(event);
	}
	output.checksum = checksum;
}

void RunAsset(WorkerData& output, Clock::time_point origin,
	const Config& c, std::uint64_t iterationsPerMs,
	std::stop_token stop)
{
	const auto end = AtMs(origin, c.durationSeconds * 1000.0);
	std::this_thread::sleep_until(origin);
	auto checksum = 0xD1B54A32D192ED03ULL + static_cast<std::uint64_t>(output.index) * 101;
	const auto iterations = IterationsForMilliseconds(c.assetWorkMs, iterationsPerMs);
	while (!stop.stop_requested() && Clock::now() < end)
	{
		checksum = Compute(iterations, checksum);
		// Учитываем только ресурсы, завершённые в измеряемом интервале.
		if (Clock::now() <= end)
			++output.completedAssets;
	}
	output.checksum = checksum;
}

struct Worker
{
	WorkerData data;
	std::atomic<int> publishedTid{ 0 };
	std::jthread thread;
};

const ThreadSettings& SettingsForRole(const Config& config, Role role)
{
	switch (role)
	{
	case Role::Audio:
		return config.audioSettings;
	case Role::Frame:
		return config.frameSettings;
	case Role::AI:
		return config.aiSettings;
	case Role::Asset:
		return config.assetSettings;
	}
	return config.assetSettings;
}

} // namespace

std::string RoleName(Role role)
{
	switch (role)
	{
	case Role::Audio:
		return "Audio";
	case Role::Frame:
		return "Frame";
	case Role::AI:
		return "AI";
	case Role::Asset:
		return "Asset";
	}
	return "unknown";
}

Config ParseArguments(int argc, char** argv, bool& calibrateOnly, bool& showHelp)
{
	Config c;
	for (int i = 1; i < argc; ++i)
	{
		const std::string key = argv[i];
		if (key == "--help" || key == "-h")
		{
			showHelp = true;
			continue;
		}
		if (key == "--calibrate")
		{
			calibrateOnly = true;
			continue;
		}
		if (i + 1 >= argc)
			throw std::invalid_argument("missing value after " + key);
		const std::string value = argv[++i];
		if (key == "--duration")
			c.durationSeconds = ParseDouble(value, key);
		else if (key == "--frame-work-ms")
			c.frameWorkMs = ParseDouble(value, key);
		else if (key == "--ai-work-ms")
			c.aiWorkMs = ParseDouble(value, key);
		else if (key == "--audio-work-ms")
			c.audioWorkMs = ParseDouble(value, key);
		else if (key == "--asset-work-ms")
			c.assetWorkMs = ParseDouble(value, key);
		else if (key == "--asset-threads")
			c.assetThreads = ParseInt(value, key);
		else if (key == "--pool-size")
			c.poolSize = ParseInt(value, key);
		else if (key == "--placement")
			c.placement = value;
		else if (key == "--audio-cpu-indices")
			c.audioCpuIndices = ParseCpuIndices(value, key);
		else if (key == "--frame-cpu-indices")
			c.frameCpuIndices = ParseCpuIndices(value, key);
		else if (key == "--ai-cpu-indices")
			c.aiCpuIndices = ParseCpuIndices(value, key);
		else if (key == "--asset-cpu-indices")
			c.assetCpuIndices = ParseCpuIndices(value, key);
		else if (key == "--iters-per-ms")
			c.iterationsPerMs = ParseUInt64(value, key);
		else if (key == "--asset-baseline-rate")
			c.baselineAssetRate = ParseDouble(value, key);
		else if (key == "--summary-csv")
			c.summaryCsv = value;
		else if (key == "--events-csv")
			c.eventsCsv = value;
		else if (key == "--audio-nice")
			c.audioSettings.nice = ParseInt(value, key);
		else if (key == "--frame-nice")
			c.frameSettings.nice = ParseInt(value, key);
		else if (key == "--ai-nice")
			c.aiSettings.nice = ParseInt(value, key);
		else if (key == "--asset-nice")
			c.assetSettings.nice = ParseInt(value, key);
		else if (key == "--audio-policy")
			c.audioSettings.policy = ParsePolicy(value);
		else if (key == "--frame-policy")
			c.frameSettings.policy = ParsePolicy(value);
		else if (key == "--ai-policy")
			c.aiSettings.policy = ParsePolicy(value);
		else if (key == "--asset-policy")
			c.assetSettings.policy = ParsePolicy(value);
		else
			throw std::invalid_argument("unknown option: " + key);
	}

	if (showHelp || calibrateOnly)
		return c;
	if (!(c.durationSeconds >= 1 && c.durationSeconds <= 120))
		throw std::invalid_argument("--duration must be in [1,120] seconds");
	if (!(c.audioWorkMs > 0 && c.audioWorkMs <= 100) || !(c.frameWorkMs > 0 && c.frameWorkMs <= 100) || !(c.aiWorkMs > 0 && c.aiWorkMs <= 100) || !(c.assetWorkMs > 0 && c.assetWorkMs <= 100))
		throw std::invalid_argument("work-ms must be in (0,100]");
	if (c.assetThreads < 1 || c.assetThreads > 16)
		throw std::invalid_argument("--asset-threads must be in [1,16]");
	if (c.poolSize < 1 || c.poolSize > CPU_SETSIZE)
		throw std::invalid_argument("invalid --pool-size");
	if (c.placement != "shared" && c.placement != "split")
		throw std::invalid_argument("--placement must be shared or split");
	if (c.placement == "split" && c.poolSize != 2)
		throw std::invalid_argument("--placement split currently requires --pool-size 2");
	for (const auto* indices : { &c.audioCpuIndices, &c.frameCpuIndices,
			 &c.aiCpuIndices, &c.assetCpuIndices })
		if (*indices)
			for (const int id : **indices)
				if (id >= c.poolSize)
					throw std::invalid_argument("CPU index must be less than --pool-size");
	for (const auto* settings : { &c.audioSettings, &c.frameSettings,
			 &c.aiSettings, &c.assetSettings })
		if (settings->nice < 0 || settings->nice > 19)
			throw std::invalid_argument("nice must be in [0,19] for an unprivileged run");
	if (c.baselineAssetRate && !(*c.baselineAssetRate > 0))
		throw std::invalid_argument("--asset-baseline-rate must be >0");
	return c;
}

void PrintHelp()
{
	std::cout << R"(game-sim -- imitation of a multithreaded game engine (Linux, C++20)

Usage: ./game-sim [options]
  --duration SECONDS         Length of the measurement (default 8)
  --audio-work-ms MS         Approximate CPU work per audio job (default 0.8)
  --frame-work-ms MS         Approximate CPU work per frame (default 3)
  --ai-work-ms MS            Approximate CPU work per AI update (default 3)
  --asset-work-ms MS         Approximate CPU work per asset (default 2)
  --asset-threads N          Background worker count (default 4)
  --pool-size N              Number of initially allowed Linux CPUs (default 1)
  --placement shared|split   Affinity preset; split needs pool-size 2
  --audio-cpu-indices LIST   Optional per-role affinity mask, pool positions
  --frame-cpu-indices LIST   For example: "0" or "0,1" (not Linux CPU IDs)
  --ai-cpu-indices LIST
  --asset-cpu-indices LIST
  --audio-nice N             Desired nice for Audio (0..19)
  --frame-nice N             Desired nice for Frame (0..19)
  --ai-nice N                Desired nice for AI (0..19)
  --asset-nice N             Desired nice for all Asset threads (0..19)
  --audio-policy other|batch|idle  Desired scheduler policy
  --frame-policy other|batch|idle
  --ai-policy other|batch|idle
  --asset-policy other|batch|idle
  --iters-per-ms N           Freeze calibrated iteration rate across runs
  --calibrate                Print iteration rate and exit
  --asset-baseline-rate N    Reference asset throughput in jobs/sec
  --summary-csv PATH         Write one-row summary CSV
  --events-csv PATH          Write periodic job events CSV
  --help                     Print this help

Default student starter only supports the unmodified baseline. Implement
ApplyThreadSettings() and ReadThreadSettings() in ThreadScheduling.cpp to enable
nice, policy and per-thread affinity experiments.

Metrics: P99 response = scheduled release to completion; missed deadlines
include skipped jobs; Q scores are pedagogical, not real-time guarantees.
)";
}

RawResults ExecuteSimulation(const Config& input)
{
	RawResults result;
	result.config = input;
	const auto available = GetAllowedCpus();
	if (result.config.poolSize > static_cast<int>(available.size()))
		throw std::runtime_error("requested CPU pool is larger than current allowed affinity mask");
	result.cpuPool.assign(available.begin(), available.begin() + result.config.poolSize);
	ConstrainMainThreadToCpuPool(result.cpuPool);

	result.iterationsPerMs = result.config.iterationsPerMs ? result.config.iterationsPerMs : CalibrateIterationsPerMillisecond();
	std::cerr << "[INFO] work calibration: " << result.iterationsPerMs
			  << " iterations/ms (fixed rate is needed for comparable experiments)\n";

	Clock::time_point origin{}; // Инициализируем после применения настроек потоков.
	const int workerCount = 3 + result.config.assetThreads;
	std::latch ready(workerCount);
	std::latch start(1);
	std::vector<std::unique_ptr<Worker>> workers;
	workers.reserve(workerCount);
	for (int i = 0; i < workerCount; ++i)
	{
		auto worker = std::make_unique<Worker>();
		if (i == 0)
			worker->data.role = Role::Audio;
		else if (i == 1)
			worker->data.role = Role::Frame;
		else if (i == 2)
			worker->data.role = Role::AI;
		else
			worker->data.role = Role::Asset;
		worker->data.index = i < 3 ? 0 : i - 3;
		auto* ptr = worker.get();
		ptr->thread = std::jthread([&, ptr](std::stop_token stopToken) {
			ptr->publishedTid.store(static_cast<int>(::syscall(SYS_gettid)), std::memory_order_release);
			ready.count_down();
			start.wait();
			if (stopToken.stop_requested())
				return;
			switch (ptr->data.role)
			{
			case Role::Audio:
				RunPeriodic(ptr->data, origin, result.config, 10.0,
					result.config.audioWorkMs, result.iterationsPerMs, stopToken);
				break;
			case Role::Frame:
				RunPeriodic(ptr->data, origin, result.config, 1000.0 / 60.0,
					result.config.frameWorkMs, result.iterationsPerMs, stopToken);
				break;
			case Role::AI:
				RunPeriodic(ptr->data, origin, result.config, 50.0,
					result.config.aiWorkMs, result.iterationsPerMs, stopToken);
				break;
			case Role::Asset:
				RunAsset(ptr->data, origin, result.config, result.iterationsPerMs, stopToken);
				break;
			}
		});
		workers.push_back(std::move(worker));
	}
	ready.wait();
	try
	{
		for (auto& worker : workers)
		{
			const auto tid = worker->publishedTid.load(std::memory_order_acquire);
			worker->data.tid = tid;
			ThreadSettings desired = SettingsForRole(result.config, worker->data.role);
			desired.cpuIds = result.cpuPool;
			if (result.config.placement == "split")
				desired.cpuIds = (worker->data.role == Role::Audio || worker->data.role == Role::Frame)
					? std::vector<int>{ result.cpuPool[0] }
					: std::vector<int>{ result.cpuPool[1] };
			const std::optional<std::vector<int>>* indices = nullptr;
			switch (worker->data.role)
			{
			case Role::Audio:
				indices = &result.config.audioCpuIndices;
				break;
			case Role::Frame:
				indices = &result.config.frameCpuIndices;
				break;
			case Role::AI:
				indices = &result.config.aiCpuIndices;
				break;
			case Role::Asset:
				indices = &result.config.assetCpuIndices;
				break;
			}
			if (indices && *indices)
			{
				desired.cpuIds.clear();
				for (int position : **indices)
					desired.cpuIds.push_back(result.cpuPool[static_cast<std::size_t>(position)]);
			}
			worker->data.requested = desired;
			try
			{
				ApplyThreadSettings(worker->thread, tid, desired);
				worker->data.observed = ReadThreadSettings(worker->thread, tid);
			}
			catch (const std::exception& e)
			{
				throw std::runtime_error("setting " + RoleName(worker->data.role) + " thread (TID="
					+ std::to_string(tid) + "): " + e.what());
			}
		}
		origin = Clock::now() + std::chrono::milliseconds(150);
		start.count_down();
	}
	catch (...)
	{
		for (auto& worker : workers)
			worker->thread.request_stop();
		start.count_down();
		throw; // деструкторы jthread выполнят join() после открытия барьера
	}
	for (auto& worker : workers)
		worker->thread.join();
	for (auto& worker : workers)
		result.workers.push_back(std::move(worker->data));
	return result;
}

} // namespace game
