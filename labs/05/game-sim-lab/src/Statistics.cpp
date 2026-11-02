#include "Statistics.h"
#include "ThreadScheduling.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

namespace game
{

namespace
{

double Percentile(std::vector<double> values, double percentile)
{
	if (values.empty())
		return 0.0;
	std::sort(values.begin(), values.end());
	const std::size_t rank = static_cast<std::size_t>(
		std::ceil(percentile * static_cast<double>(values.size())));
	return values[std::max<std::size_t>(1, rank) - 1];
}

double Average(const std::vector<double>& values)
{
	if (values.empty())
		return 0.0;
	return std::accumulate(values.begin(), values.end(), 0.0) / values.size();
}

PeriodicMetrics Periodic(const WorkerData& w, double periodMs, double workMs)
{
	PeriodicMetrics m;
	m.name = RoleName(w.role);
	m.deadlineMs = periodMs;
	m.plannedWorkMs = workMs;
	std::vector<double> responses;
	std::vector<double> wakeLatencies;
	std::vector<double> cpuWork;
	for (const auto& e : w.events)
	{
		++m.scheduled;
		if (e.skipped)
		{
			++m.skipped;
			continue;
		}
		++m.completed;
		if (e.missedDeadline)
			++m.late;
		if (e.startMs && e.finishMs)
		{
			responses.push_back(*e.finishMs - e.releaseMs);
			wakeLatencies.push_back(std::max(0.0, *e.startMs - e.releaseMs));
			cpuWork.push_back(e.cpuMs);
		}
	}
	if (m.scheduled > 0)
		m.missPercent = 100.0 * static_cast<double>(m.late + m.skipped)
			/ static_cast<double>(m.scheduled);
	m.avgResponseMs = Average(responses);
	m.p99ResponseMs = Percentile(responses, 0.99);
	m.p99WakeLatenessMs = Percentile(wakeLatencies, 0.99);
	m.avgCpuMs = Average(cpuWork);
	// Учебная функция качества, не гарантия планируемости:
	// Q = 100 * clip((D - P99_response)/(D - C_reference), 0, 1)
	// Пропущенные запуски считаются нарушениями дедлайна.
	if (m.completed == 0 || workMs >= periodMs)
		m.quality = 0.0;
	else
		m.quality = std::clamp(100.0 * (periodMs - m.p99ResponseMs)
				/ (periodMs - workMs),
			0.0, 100.0);
	return m;
}

const WorkerData& FindPeriodic(const RawResults& data, Role role)
{
	auto it = std::find_if(data.workers.begin(), data.workers.end(),
		[role](const auto& w) { return w.role == role; });
	if (it == data.workers.end())
		throw std::logic_error("periodic worker missing");
	return *it;
}

std::string CpuList(const std::vector<int>& cpus)
{
	std::string out;
	for (int id : cpus)
	{
		if (!out.empty())
			out += ';';
		out += std::to_string(id);
	}
	return out;
}

void PeriodicRow(std::ofstream& f, const PeriodicMetrics& m)
{
	f << ',' << m.scheduled << ',' << m.completed << ',' << m.skipped
	  << ',' << m.late << ',' << m.missPercent << ',' << m.avgResponseMs
	  << ',' << m.p99ResponseMs << ',' << m.p99WakeLatenessMs
	  << ',' << m.avgCpuMs << ',' << m.quality;
}

} // namespace

Report Evaluate(const RawResults& data)
{
	Report r;
	r.audio = Periodic(FindPeriodic(data, Role::Audio), 10.0, data.config.audioWorkMs);
	r.frame = Periodic(FindPeriodic(data, Role::Frame), 1000.0 / 60.0, data.config.frameWorkMs);
	r.ai = Periodic(FindPeriodic(data, Role::AI), 50.0, data.config.aiWorkMs);
	for (const auto& w : data.workers)
		if (w.role == Role::Asset)
			r.assetJobs += w.completedAssets;
	r.assetJobsPerSecond = r.assetJobs / data.config.durationSeconds;
	if (data.config.baselineAssetRate)
	{
		r.assetBaselineAvailable = true;
		r.assetRelativePercent = 100.0 * r.assetJobsPerSecond
			/ *data.config.baselineAssetRate;
		r.assetAcceptable = r.assetRelativePercent >= 80.0;
	}
	r.gameScore = 0.40 * r.audio.quality + 0.45 * r.frame.quality + 0.15 * r.ai.quality;
	r.periodicAcceptable = r.audio.missPercent <= 0.1 && r.frame.missPercent <= 1.0
		&& r.ai.missPercent <= 5.0;
	return r;
}

void PrintReport(const RawResults& data, const Report& r)
{
	std::cout << std::fixed << std::setprecision(3);
	std::cout << "\n=== Game simulation ===\n";
	std::cout << "Duration: " << data.config.durationSeconds << " s; CPU pool: "
			  << CpuList(data.cpuPool) << "; placement: " << data.config.placement << '\n';
	std::cout << "Workload (approx. CPU ms/job): audio=" << data.config.audioWorkMs
			  << " frame=" << data.config.frameWorkMs << " ai=" << data.config.aiWorkMs
			  << " asset=" << data.config.assetWorkMs << '\n';
	std::cout << "Frozen iterations/ms: " << data.iterationsPerMs << '\n';
	std::cout << "\nRequested / observed thread settings:\n";
	for (const auto& w : data.workers)
	{
		std::cout << "  " << RoleName(w.role) << '#' << w.index << " TID=" << w.tid
				  << " wanted=" << PolicyName(w.requested.policy)
				  << "/nice=" << w.requested.nice
				  << "/cpus=" << CpuList(w.requested.cpuIds);
		if (w.observed)
			std::cout << " observed=" << PolicyName(w.observed->policy)
					  << "/nice=" << w.observed->nice
					  << "/prio=" << w.observed->priority
					  << "/cpus=" << CpuList(w.observed->cpuIds);
		else
			std::cout << " observed=NOT_IMPLEMENTED";
		std::cout << '\n';
	}
	std::cout << "\nPeriodic jobs (response = scheduled release to completion):\n";
	std::cout << "  Role  Planned Done Skipped Late Miss%  P99-response(ms) P99-wake(ms) Avg-CPU(ms) Q/100\n";
	for (const auto* m : { &r.audio, &r.frame, &r.ai })
		std::cout << "  " << m->name << " " << m->scheduled << " " << m->completed << " "
				  << m->skipped << " " << m->late << " " << m->missPercent << " "
				  << m->p99ResponseMs << " " << m->p99WakeLatenessMs << " "
				  << m->avgCpuMs << " " << m->quality << '\n';
	std::uint64_t checksum = 0;
	for (const auto& worker : data.workers)
		checksum ^= worker.checksum;
	std::cout << "CHECKSUM_XOR=" << checksum << '\n';
	std::cout << "\nASSET_THROUGHPUT=" << r.assetJobsPerSecond << '\n';
	std::cout << "GAME_QUALITY_SCORE=" << r.gameScore << '\n';
	if (r.assetBaselineAvailable)
	{
		std::cout << "ASSET_RELATIVE_PERCENT=" << r.assetRelativePercent << '\n';
		std::cout << "ACCEPTABILITY="
				  << (r.periodicAcceptable && r.assetAcceptable ? "PASS" : "FAIL") << '\n';
	}
	else
	{
		std::cout << "ASSET_RELATIVE_PERCENT=N/A\n";
		std::cout << "ACCEPTABILITY=BASELINE_REQUIRED\n";
	}
	if (!r.periodicAcceptable)
		std::cout << "[WARNING] Periodic miss-rate limits exceeded.\n";
	if (r.assetBaselineAvailable && !r.assetAcceptable)
		std::cout << "[WARNING] Asset throughput below 80% of reference.\n";
	std::cout << "[NOTE] Q is an educational score. Missed jobs are counted over all scheduled releases; "
				 "P99 uses completed jobs only.\n";
}

void WriteCsvFiles(const RawResults& data, const Report& r)
{
	if (!data.config.summaryCsv.empty())
	{
		std::ofstream f(data.config.summaryCsv);
		if (!f)
			throw std::runtime_error("cannot open summary CSV: " + data.config.summaryCsv);
		f << "duration_s,cpus,placement,iters_per_ms,audio_work_ms,frame_work_ms,ai_work_ms,asset_work_ms,"
			 "asset_threads,audio_nice,frame_nice,ai_nice,asset_nice,audio_policy,frame_policy,ai_policy,asset_policy,";
		for (const char* name : { "audio", "frame", "ai" })
			f << name << "_planned," << name << "_completed," << name << "_skipped," << name << "_late,"
			  << name << "_miss_pct," << name << "_avg_response_ms," << name << "_p99_response_ms,"
			  << name << "_p99_wake_ms," << name << "_avg_cpu_ms," << name << "_q,";
		f << "asset_jobs,asset_jobs_per_s,asset_baseline_jobs_per_s,asset_relative_pct,"
			 "periodic_pass,asset_pass,overall_status,game_score\n";
		f << std::fixed << std::setprecision(6);
		f << data.config.durationSeconds << ",\"" << CpuList(data.cpuPool) << "\","
		  << data.config.placement << ',' << data.iterationsPerMs << ','
		  << data.config.audioWorkMs << ',' << data.config.frameWorkMs << ','
		  << data.config.aiWorkMs << ',' << data.config.assetWorkMs << ','
		  << data.config.assetThreads << ',' << data.config.audioSettings.nice << ','
		  << data.config.frameSettings.nice << ',' << data.config.aiSettings.nice << ','
		  << data.config.assetSettings.nice << ','
		  << PolicyName(data.config.audioSettings.policy) << ','
		  << PolicyName(data.config.frameSettings.policy) << ','
		  << PolicyName(data.config.aiSettings.policy) << ','
		  << PolicyName(data.config.assetSettings.policy);
		PeriodicRow(f, r.audio);
		PeriodicRow(f, r.frame);
		PeriodicRow(f, r.ai);
		f << ',' << r.assetJobs << ',' << r.assetJobsPerSecond << ',';
		if (data.config.baselineAssetRate)
			f << *data.config.baselineAssetRate;
		f << ',';
		if (r.assetBaselineAvailable)
			f << r.assetRelativePercent;
		f << ',' << (r.periodicAcceptable ? 1 : 0) << ',';
		if (r.assetBaselineAvailable)
			f << (r.assetAcceptable ? "1" : "0");
		f << ',' << (r.assetBaselineAvailable ? (r.periodicAcceptable && r.assetAcceptable ? "PASS" : "FAIL") : "BASELINE_REQUIRED")
		  << ',' << r.gameScore << '\n';
	}
	if (!data.config.eventsCsv.empty())
	{
		std::ofstream f(data.config.eventsCsv);
		if (!f)
			throw std::runtime_error("cannot open events CSV: " + data.config.eventsCsv);
		f << "role,job_index,release_ms,start_ms,finish_ms,response_ms,cpu_ms,missed,skipped\n";
		f << std::fixed << std::setprecision(6);
		for (const auto& w : data.workers)
		{
			if (w.role == Role::Asset)
				continue;
			for (const auto& e : w.events)
			{
				f << RoleName(w.role) << ',' << e.index << ',' << e.releaseMs << ',';
				if (e.startMs)
					f << *e.startMs;
				f << ',';
				if (e.finishMs)
					f << *e.finishMs;
				f << ',';
				if (e.finishMs)
					f << (*e.finishMs - e.releaseMs);
				f << ',' << e.cpuMs << ',' << (e.missedDeadline ? 1 : 0)
				  << ',' << (e.skipped ? 1 : 0) << '\n';
			}
		}
	}
}

} // namespace game
