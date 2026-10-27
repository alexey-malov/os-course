#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <time.h>
#include <unistd.h>
#include <vector>

namespace
{

struct Message
{
	std::uint64_t sequence;
	std::int64_t sentNs;
};
static_assert(sizeof(Message) == 16);

struct Options
{
	bool producer = false;
	bool consumer = false;
	std::uint64_t periodMs = 10;
	std::uint64_t messages = 300;
};

void PrintUsage()
{
	std::cerr << "Usage:\n"
			  << "  io-pipe --producer --period-ms 10 --messages 300 | io-pipe --consumer\n"
			  << "  stdout of producer is a BINARY data stream; do not print it.\n";
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
		std::string_view arg = argv[i];
		if (arg == "--help")
		{
			PrintUsage();
			std::exit(0);
		}
		if (arg == "--producer")
		{
			o.producer = true;
			continue;
		}
		if (arg == "--consumer")
		{
			o.consumer = true;
			continue;
		}
		if (i + 1 >= argc)
		{
			throw std::invalid_argument("Missing value after " + std::string(arg));
		}
		const auto value = ParseUInt(argv[++i]);
		if (arg == "--period-ms")
		{
			o.periodMs = value;
		}
		else if (arg == "--messages")
		{
			o.messages = value;
		}
		else
		{
			throw std::invalid_argument("Unknown option: " + std::string(arg));
		}
	}
	if (o.producer == o.consumer || o.periodMs == 0 || o.messages == 0)
	{
		throw std::invalid_argument("Use either --producer or --consumer, with positive values");
	}
	if (o.periodMs > 60'000 || o.messages > 1'000'000)
	{
		throw std::invalid_argument("Use --period-ms <= 60000 and --messages <= 1000000");
	}
	return o;
}

std::int64_t MonotonicNs()
{
	::timespec ts{};
	if (::clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
	{
		throw std::runtime_error(std::string("clock_gettime: ") + std::strerror(errno));
	}
	return static_cast<std::int64_t>(ts.tv_sec) * 1'000'000'000LL + ts.tv_nsec;
}

void SleepUntil(std::int64_t targetNs)
{
	const ::timespec ts{ static_cast<time_t>(targetNs / 1'000'000'000LL),
		static_cast<long>(targetNs % 1'000'000'000LL) };
	for (;;)
	{
		const int result = ::clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, nullptr);
		if (result == 0)
		{
			return;
		}
		if (result != EINTR)
		{
			throw std::runtime_error(std::string("clock_nanosleep: ") + std::strerror(result));
		}
	}
}

void WriteMessage(const Message& message)
{
	const auto* bytes = reinterpret_cast<const unsigned char*>(&message);
	std::size_t sent = 0;
	while (sent < sizeof(message))
	{
		const auto n = ::write(STDOUT_FILENO, bytes + sent, sizeof(message) - sent);
		if (n < 0 && errno == EINTR)
		{
			continue;
		}
		if (n <= 0)
		{
			throw std::runtime_error(std::string("pipe write failed: ") + std::strerror(errno));
		}
		sent += static_cast<std::size_t>(n);
	}
}

bool ReadMessage(Message& message)
{
	auto* bytes = reinterpret_cast<unsigned char*>(&message);
	std::size_t received = 0;
	while (received < sizeof(message))
	{
		const auto n = ::read(STDIN_FILENO, bytes + received, sizeof(message) - received);
		if (n < 0 && errno == EINTR)
		{
			continue;
		}
		if (n < 0)
		{
			throw std::runtime_error(std::string("pipe read failed: ") + std::strerror(errno));
		}
		if (n == 0)
		{
			if (received == 0)
			{
				return false;
			}
			throw std::runtime_error("Unexpected EOF in the middle of a message");
		}
		received += static_cast<std::size_t>(n);
	}
	return true;
}

double Percentile(const std::vector<double>& sortedValues, double probability)
{
	// Процентиль определяем по ближайшему верхнему рангу: ceil(p * N).
	const auto rank = static_cast<std::size_t>(std::ceil(probability * sortedValues.size()));
	const auto index = std::min(sortedValues.size() - 1,
		(rank == 0 ? std::size_t{ 0 } : rank - 1));
	return sortedValues[index];
}

void Produce(const Options& o)
{
	const auto baseNs = MonotonicNs();
	const auto periodNs = static_cast<std::int64_t>(o.periodMs) * 1'000'000LL;
	std::vector<double> wakeupLateMs;
	wakeupLateMs.reserve(static_cast<std::size_t>(o.messages));

	for (std::uint64_t i = 0; i < o.messages; ++i)
	{
		// Абсолютное время пробуждения исключает накопление дрейфа.
		const auto plannedNs = baseNs + static_cast<std::int64_t>(i) * periodNs;
		SleepUntil(plannedNs);
		const auto sendNs = MonotonicNs();
		const auto lateNs = std::max<std::int64_t>(0, sendNs - plannedNs);
		wakeupLateMs.push_back(static_cast<double>(lateNs) / 1'000'000.0);
		WriteMessage(Message{ i, sendNs });
	}

	std::sort(wakeupLateMs.begin(), wakeupLateMs.end());
	// Диагностика Producer идёт в stderr: в stdout только бинарные сообщения.
	std::cerr << std::fixed << std::setprecision(3)
			  << "producer_messages=" << o.messages << '\n'
			  << "period_ms=" << o.periodMs << '\n'
			  << "wakeup_p99_ms=" << Percentile(wakeupLateMs, 0.99) << '\n'
			  << "wakeup_max_ms=" << wakeupLateMs.back() << '\n';
}

void Consume()
{
	std::vector<double> latencyMs;
	Message message{};
	std::uint64_t expectedSequence = 0;
	while (ReadMessage(message))
	{
		const auto receivedNs = MonotonicNs();
		if (message.sequence != expectedSequence)
		{
			throw std::runtime_error("Unexpected sequence number; incompatible binary input?");
		}
		++expectedSequence;
		// Эта задержка включает расходы IPC и ожидание процессора.
		const auto deltaNs = std::max<std::int64_t>(0, receivedNs - message.sentNs);
		latencyMs.push_back(static_cast<double>(deltaNs) / 1'000'000.0);
	}
	if (latencyMs.empty())
	{
		throw std::runtime_error("No messages received");
	}

	long double total = 0;
	for (const auto value : latencyMs)
	{
		total += value;
	}
	std::sort(latencyMs.begin(), latencyMs.end());
	const auto average = static_cast<double>(total / latencyMs.size());
	std::cout << std::fixed << std::setprecision(3)
			  << "mode=consumer\n"
			  << "messages_received=" << latencyMs.size() << '\n'
			  << "latency_mean_ms=" << average << '\n'
			  << "latency_p50_ms=" << Percentile(latencyMs, 0.50) << '\n'
			  << "latency_p95_ms=" << Percentile(latencyMs, 0.95) << '\n'
			  << "latency_p99_ms=" << Percentile(latencyMs, 0.99) << '\n'
			  << "latency_max_ms=" << latencyMs.back() << '\n';
}

} // namespace

int main(int argc, char** argv)
{
	try
	{
		const auto options = ParseArgs(argc, argv);
		if (options.producer)
		{
			Produce(options);
		}
		else
		{
			Consume();
		}
		return 0;
	}
	catch (const std::exception& e)
	{
		std::cerr << "io-pipe: " << e.what() << '\n';
		PrintUsage();
		return 2;
	}
}
