#include "ThreadScheduling.h"

#include <algorithm>
#include <pthread.h>
#include <sched.h>
#include <sstream>
#include <stdexcept>
#include <sys/resource.h>

namespace game
{

std::string PolicyName(int policy)
{
	switch (policy)
	{
	case SCHED_OTHER:
		return "other";
	case SCHED_BATCH:
		return "batch";
	case SCHED_IDLE:
		return "idle";
	case SCHED_FIFO:
		return "fifo";
	case SCHED_RR:
		return "rr";
	default:
		return "unknown(" + std::to_string(policy) + ")";
	}
}

int ParsePolicy(const std::string& value)
{
	if (value == "other")
		return SCHED_OTHER;
	if (value == "batch")
		return SCHED_BATCH;
	if (value == "idle")
		return SCHED_IDLE;
	// В обязательной части намеренно запрещены политики реального времени.
	throw std::invalid_argument("policy must be one of: other, batch, idle");
}

void ApplyThreadSettings(std::jthread& thread, pid_t tid,
	const ThreadSettings& desired)
{
	// TODO (студент):
	// 1. Получите pthread_t через thread.native_handle() и вызовите
	//    pthread_setschedparam(). Для OTHER/BATCH/IDLE приоритет равен 0.
	// 2. Установите cpuIds через pthread_setaffinity_np(), CPU_ZERO(), CPU_SET().
	// 3. Установите nice через setpriority(PRIO_PROCESS, tid, desired.nice).
	// 4. Проверяйте ошибки: pthread_* возвращают код ошибки напрямую,
	//    а системные вызовы используют errno.
	// 5. Не путайте pthread_t и числовой Linux TID.
	// Заготовка должна запускаться в базовой конфигурации.
	// Поэтому здесь допускаются настройки, совпадающие с текущими,
	// но реальное изменение запрещено, пока TODO не реализован.
	(void)tid;
	if (desired.policy == SCHED_OTHER && desired.nice == 0)
	{
		cpu_set_t actual{};
		CPU_ZERO(&actual);
		const int rc = pthread_getaffinity_np(thread.native_handle(),
			sizeof(actual), &actual);
		if (rc == 0)
		{
			bool matches = true;
			for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu)
			{
				const bool present = CPU_ISSET(cpu, &actual);
				const bool desiredPresent = std::find(desired.cpuIds.begin(), desired.cpuIds.end(), cpu)
					!= desired.cpuIds.end();
				if (present != desiredPresent)
				{
					matches = false;
					break;
				}
			}
			if (matches)
				return;
		}
	}
	throw std::runtime_error("TODO: implement ApplyThreadSettings in src/ThreadScheduling.cpp");
}

std::optional<ObservedThreadSettings> ReadThreadSettings(std::jthread& thread,
	pid_t tid)
{
	// TODO (студент):
	// 1. pthread_getschedparam() -> policy и sched_priority.
	// 2. pthread_getaffinity_np() -> список разрешённых CPU.
	// 3. getpriority(PRIO_PROCESS, tid) -> nice (обнулите/проверьте errno).
	// 4. Верните фактические, а не только запрошенные настройки.
	(void)thread;
	(void)tid;
	return std::nullopt;
}

} // namespace game
