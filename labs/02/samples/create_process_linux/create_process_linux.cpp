#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sys/wait.h>
#include <unistd.h>

int main()
{
	pid_t pid = fork();
	if (pid < 0)
	{
		std::cerr << "fork failed: " << std::strerror(errno) << "\n";
		return 1;
	}

	if (pid == 0)
	{ // child
		// В C++ создаём МОДИФИЦИРУЕМЫЕ массивы символов вместо литералов:
		char cmd[] = "ls";
		char arg1[] = "-l";
		char arg2[] = "/tmp";
		char* args[] = { cmd, arg1, arg2, nullptr };

// Опционально перенаправляем вывод в файл, как это делается в оболочке:
#if 1
		int fd = open("out.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644);
		dup2(fd, STDOUT_FILENO);
		close(fd);
#endif

		execvp(cmd, args); // заменить образ процесса
		std::cerr << "execvp failed: " << std::strerror(errno) << "\n";

		// Выходим при помощи _exit, а не exit, чтобы не вызывать обработчики atexit,
		// которые могут быть не в состоянии корректно выполниться в дочернем процессе.
		_exit(127); // сюда попадаем только при ошибке exec
	}
	else
	{ // parent
		int status = 0;
		if (waitpid(pid, &status, 0) < 0)
		{
			std::cerr << "waitpid failed: " << std::strerror(errno) << "\n";
			return 1;
		}
		// Проверяем, завершился ли процесс с кодом возврата?
		if (WIFEXITED(status))
		{
			std::cout << "child exit code = " << WEXITSTATUS(status) << "\n";
		}
		else if (WIFSIGNALED(status)) // // Проверяем, завершился ли процесс сигналом
		{
			std::cout << "child killed by signal " << WTERMSIG(status) << "\n";
		}
	}
	return 0;
}
