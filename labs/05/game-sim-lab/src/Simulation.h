#pragma once

#include "ThreadScheduling.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace game
{

using Clock = std::chrono::steady_clock;

enum class Role
{
	Audio,
	Frame,
	AI,
	Asset
};
std::string RoleName(Role role);

struct Config
{
	double durationSeconds = 8.0;
	double audioWorkMs = 0.8;
	double frameWorkMs = 3.0;
	double aiWorkMs = 3.0;
	double assetWorkMs = 2.0;
	int assetThreads = 4;
	int poolSize = 1;
	std::string placement = "shared"; // shared / split
	// Индивидуальные маски: индексы CPU внутри выбранного пула, начиная с нуля.
	std::optional<std::vector<int>> audioCpuIndices;
	std::optional<std::vector<int>> frameCpuIndices;
	std::optional<std::vector<int>> aiCpuIndices;
	std::optional<std::vector<int>> assetCpuIndices;
	std::uint64_t iterationsPerMs = 0; // 0 означает автоматическую калибровку
	std::optional<double> baselineAssetRate;
	std::string summaryCsv;
	std::string eventsCsv;

	ThreadSettings audioSettings;
	ThreadSettings frameSettings;
	ThreadSettings aiSettings;
	ThreadSettings assetSettings;
};

struct Event
{
	std::uint64_t index = 0;
	double releaseMs = 0.0;
	std::optional<double> startMs;
	std::optional<double> finishMs;
	double cpuMs = 0.0;
	bool missedDeadline = false;
	bool skipped = false;
};

struct WorkerData
{
	Role role = Role::Asset;
	int index = 0;
	int tid = 0;
	ThreadSettings requested;
	std::vector<Event> events;
	std::uint64_t completedAssets = 0;
	std::uint64_t checksum = 0;
	std::optional<ObservedThreadSettings> observed;
};

struct RawResults
{
	Config config;
	std::vector<int> cpuPool;
	std::vector<WorkerData> workers;
	std::uint64_t iterationsPerMs = 0;
};

Config ParseArguments(int argc, char** argv, bool& calibrateOnly, bool& showHelp);
void PrintHelp();
RawResults ExecuteSimulation(const Config& config);

} // namespace game
