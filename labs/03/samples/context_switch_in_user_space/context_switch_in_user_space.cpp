#include <boost/context/fiber.hpp>
#include <iostream>
#include <utility>

namespace ctx = boost::context;

ctx::fiber Worker(ctx::fiber&& mainContext)
{
	int value = 42;

	std::cout << "worker: value = " << value << '\n';
	std::cout << "worker: yield to main\n";

	// Передаём управление обратно в main.
	mainContext = std::move(mainContext).resume();

	// Когда main снова вызовет worker.resume(),
	// выполнение продолжится отсюда.
	++value;

	std::cout << "worker: resumed\n";
	std::cout << "worker: value = " << value << '\n';

	// Завершаем worker и возвращаем управление в main.
	return std::move(mainContext);
}

int main()
{
	std::cout << "main: start\n";

	ctx::fiber worker{ Worker };

	std::cout << "main: resume worker\n";

	worker = std::move(worker).resume();

	std::cout << "main: worker yielded\n";
	std::cout << "main: resume worker again\n";

	worker = std::move(worker).resume();

	std::cout << "main: worker finished\n";
}