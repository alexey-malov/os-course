#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

//
static volatile sig_atomic_t g_shouldStop = 0;

void HandleSignal(int sig)
{
	if (sig == SIGTERM || sig == SIGINT)
		g_shouldStop = 1;
}

int Daemonize()
{
	// 1) Первый fork
	pid_t pid = fork();
	if (pid < 0)
		return -1; // ошибка
	if (pid > 0)
		_exit(0); // родитель сразу выходит

	// 2) Создаём новую сессию, чтобы отвязаться от терминала
	if (setsid() < 0)
		return -1;

	// 3) Игнорируем SIGHUP (разрыв соединения с терминалом),
	// чтобы не завершться при потере управляющего терминала
	std::signal(SIGHUP, SIG_IGN);

	// 4) Второй fork гарантирует, что лидер сессии не сможет
	// снова получить управляющий терминал
	pid = fork();
	if (pid < 0)
		return -1; // ошибка
	if (pid > 0)
		_exit(0); // родитель второй стадии выходит

	// 5) Задаём маску rw-r----- по умолчанию для новых файлов (базовые права 0666)
	::umask(027);
	// Выходим в корневой каталог, чтобы не держать текущий каталог занятым
	if (chdir("/") != 0)
	{
	}

	// 6) Закрыть стандартные дескрипторы (у демона нет терминала)
	// В учебных примерах мы полагаемся на особенность работы системного вызова open():
	// он всегда возвращает минимально доступный номер файлового дескриптора.
	// Поэтому порядок здесь имеет значение:
	//   1) мы сначала закрываем стандартные дескрипторы stdin (0), stdout (1) и stderr (2),
	//   2) затем первым open() открываем /dev/null для чтения — он получит номер 0 (stdin),
	//   3) затем ещё дважды открываем /dev/null для записи — они займут 1 и 2 (stdout и stderr).
	//
	// В результате стандартные потоки перенаправляются в /dev/null автоматически,
	// без явных вызовов dup2(). В продакшене чаще stdout и stderr перенаправляют в лог-файл
	// или syslog, чтобы не терять сообщения, но для демонстрационного кода достаточно /dev/null.
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	// Переоткрыть на /dev/null
	[[maybe_unused]] int fd0 = open("/dev/null", O_RDONLY);
	[[maybe_unused]] int fd1 = open("/dev/null", O_WRONLY);
	[[maybe_unused]] int fd2 = open("/dev/null", O_WRONLY);
	return 0;
}

int main()
{
	if (Daemonize() != 0)
	{
		// Если нужно — вывести ошибку перед тем как уйти в фон:
		// но stdout уже закрыт, поэтому можем кратко написать в syslog
		openlog("mydaemon", LOG_PID | LOG_CONS, LOG_DAEMON);
		syslog(LOG_ERR, "Daemonize failed: %s", std::strerror(errno));
		closelog();
		return EXIT_FAILURE;
	}

	// Логирование через syslog
	openlog("mydaemon", LOG_PID, LOG_DAEMON);

	// Обработка сигналов для корректного завершения
	std::signal(SIGTERM, HandleSignal);
	std::signal(SIGINT, HandleSignal);

	// Выводим
	syslog(LOG_INFO, "started");

	// Простейшая «работа демона»: периодически что-то делать
	while (!g_shouldStop)
	{
		syslog(LOG_INFO, "tick");
		sleep(5);
	}

	syslog(LOG_INFO, "stopping");
	closelog();
	return EXIT_SUCCESS;
}
