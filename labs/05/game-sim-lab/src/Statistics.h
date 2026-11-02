#pragma once

#include "Simulation.h"

namespace game
{

struct PeriodicMetrics
{
	std::string name;
	double deadlineMs = 0;
	double plannedWorkMs = 0;
	std::uint64_t scheduled = 0;
	std::uint64_t completed = 0;
	std::uint64_t skipped = 0;
	std::uint64_t late = 0; // завершено позже дедлайна
	double missPercent = 0;
	double avgResponseMs = 0;
	double p99ResponseMs = 0;
	double p99WakeLatenessMs = 0;
	double avgCpuMs = 0;
	double quality = 0;
};

struct Report
{
	PeriodicMetrics audio;
	PeriodicMetrics frame;
	PeriodicMetrics ai;
	std::uint64_t assetJobs = 0;
	double assetJobsPerSecond = 0;
	double assetRelativePercent = 0;
	bool assetBaselineAvailable = false;
	double gameScore = 0;
	bool periodicAcceptable = false;
	bool assetAcceptable = false;
};

Report Evaluate(const RawResults& data);
void PrintReport(const RawResults& data, const Report& report);
void WriteCsvFiles(const RawResults& data, const Report& report);

} // namespace game
