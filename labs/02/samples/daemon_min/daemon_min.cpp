#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int main()
{
	pid_t pid = fork();
	if (pid > 0)
		exit(0); // Завершаем родителя
	setsid(); // Новый сессионный лидер
	chdir("/"); // Работаем из корня
	umask(027); // Маска прав для создаваемых файлов (rw-r-----)

	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	open("/dev/null", O_RDONLY);
	open("/dev/null", O_WRONLY);
	open("/dev/null", O_WRONLY);

	for (;;)
	{
		// Основная работа демона
		sleep(60);
	}
}
