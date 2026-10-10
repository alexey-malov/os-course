#include <boost/context/fiber.hpp>
#include <iostream>
#include <utility>

namespace ctx = boost::context;

// Функция выполняется внутри отдельного fiber.
//
// У fiber есть собственный стек выполнения.
// Параметр mainContext представляет контекст, из которого был запущен
// этот fiber. Через него worker сможет вернуть управление обратно в main.
ctx::fiber Worker(ctx::fiber&& mainContext)
{
	// Локальная переменная находится в стеке worker.
	// После переключения на main стек worker сохраняется,
	// поэтому при последующем возобновлении value останется равным 42.
	int value = 42;

	std::cout << "worker: value = " << value << '\n';
	std::cout << "worker: yield to main\n";

	// Передаём управление обратно в main.
	//
	// В этот момент выполнение Worker приостанавливается:
	// - сохраняется текущая позиция выполнения;
	// - сохраняется стек worker;
	// - восстанавливается контекст main.
	//
	// Когда main позднее снова возобновит worker,
	// выполнение продолжится со следующей строки.
	//
	// resume() возвращает новый handle текущего контекста,
	// поэтому его необходимо сохранить обратно в mainContext.
	mainContext = std::move(mainContext).resume();

	// Мы попадём сюда только после того, как main снова вызовет
	// worker.resume().
	//
	// Стек worker был сохранён, поэтому value по-прежнему существует
	// и имеет значение 42.
	++value;

	std::cout << "worker: resumed\n";
	std::cout << "worker: value = " << value << '\n';

	// Функция worker заканчивается.
	//
	// Возвращаем контекст main, чтобы Boost.Context мог передать
	// управление обратно в main.
	return std::move(mainContext);
}

int main()
{
	std::cout << "main: start\n";

	// Создаём fiber.
	//
	// Для worker будет создан отдельный стек.
	// Однако Worker пока не начинает выполняться:
	// выполнение начнётся только после первого resume().
	ctx::fiber worker{ Worker };

	std::cout << "main: resume worker\n";

	worker = std::move(worker).resume();

	std::cout << "main: worker yielded\n";
	std::cout << "main: resume worker again\n";

	worker = std::move(worker).resume();

	std::cout << "main: worker finished\n";
}