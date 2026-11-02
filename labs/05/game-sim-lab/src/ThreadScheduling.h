#pragma once

#include <optional>
#include <sched.h>
#include <string>
#include <sys/types.h>
#include <thread>
#include <vector>

namespace game
{

struct ThreadSettings
{
	int policy = SCHED_OTHER;
	int nice = 0;
	std::vector<int> cpuIds;
};

struct ObservedThreadSettings
{
	int policy = SCHED_OTHER;
	int priority = 0;
	int nice = 0;
	std::vector<int> cpuIds;
};

// ЗАДАНИЕ СТУДЕНТА 1: реализовать в ThreadScheduling.cpp.
// native_handle() позволяет получить pthread_t в используемой Linux-среде.
// tid — Linux TID, полученный внутри рабочего потока через gettid().
// При ошибке операции выбрасывайте исключение с понятным описанием.
void ApplyThreadSettings(std::jthread& thread, pid_t tid,
	const ThreadSettings& desired);

// ЗАДАНИЕ СТУДЕНТА 2: получить настоящие настройки от ядра.
std::optional<ObservedThreadSettings> ReadThreadSettings(std::jthread& thread,
	pid_t tid);

std::string PolicyName(int policy);
int ParsePolicy(const std::string& name);

} // namespace game
