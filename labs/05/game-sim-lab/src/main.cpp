#include "Simulation.h"
#include "Statistics.h"
#include "Workload.h"

#include <exception>
#include <iostream>

int main(int argc, char** argv)
{
	try
	{
		bool calibrateOnly = false;
		bool showHelp = false;
		const game::Config config = game::ParseArguments(argc, argv, calibrateOnly, showHelp);
		if (showHelp)
		{
			game::PrintHelp();
			return 0;
		}
		if (calibrateOnly)
		{
			std::cout << game::CalibrateIterationsPerMillisecond() << '\n';
			return 0;
		}

		auto data = game::ExecuteSimulation(config);
		auto report = game::Evaluate(data);
		game::PrintReport(data, report);
		game::WriteCsvFiles(data, report);
		return 0; // Нарушение дедлайнов является результатом эксперимента, а не ошибкой программы.
	}
	catch (const std::exception& error)
	{
		std::cerr << "ERROR: " << error.what() << '\n';
		return 1;
	}
}
