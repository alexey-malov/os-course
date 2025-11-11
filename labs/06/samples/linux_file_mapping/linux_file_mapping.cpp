#include "../lib/scope_guards.h"
#include <cstring> // std::memcpy
#include <fcntl.h> // open
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/mman.h> // mmap, msync, munmap
#include <sys/stat.h> // fstat
#include <unistd.h> // ftruncate, close

constexpr std::size_t FileSize = 64 * 1024; // 64 KiB
constexpr std::size_t Offset = 16 * 1024; // 16 KiB
const std::string payload = "MMAP_WAS_HERE";

void PrepareFileUsingMapping(const char* path)
{
	// 1) Создаём/обнуляем файл нужного размера
	int fd = ::open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
	if (fd == -1)
	{
		throw std::runtime_error("Failed to open file");
	}

	// Создаём scope-guard, который при выходе из блока вызовет close
	auto closeFd = sg::MakeScopeExit([fd]() noexcept { ::close(fd); });

	if (::ftruncate(fd, static_cast<off_t>(FileSize)) == -1)
	{
		throw std::runtime_error("Failed to resize file");
	}

	// 2) Отображаем файл целиком в память (RW, MAP_SHARED)
	void* addr = ::mmap(nullptr, FileSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (addr == MAP_FAILED)
	{
		throw std::runtime_error("Failed to mmap file");
	}

	// Этот scope guard вызовет munmap при выходе из блока
	auto unmapAddr = sg::MakeScopeExit([addr]() noexcept {
		if (::munmap(addr, FileSize) == -1)
		{
			perror("munmap failed");
		}
	});

	// 3) Пишем данные, начиная со смещения 16 KiB
	char* base = static_cast<char*>(addr);
	std::memcpy(base + Offset, payload.data(), payload.size());

	// 4) Синхронизация изменённого диапазона на диск и размэпливание
	if (::msync(base + Offset, payload.size(), MS_SYNC) == -1)
	{
		throw std::runtime_error("Failed to msync file");
	}
}

bool VerifyFileContentWithIostream(const char* path)
{
	// 5) Проверяем через std::fstream
	std::ifstream in(path, std::ios::binary);
	if (!in)
	{
		throw std::runtime_error("Failed to open file via fstream");
	}

	in.seekg(static_cast<std::streamoff>(Offset), std::ios::beg);
	std::string got(payload.size(), '\0');
	in.read(got.data(), static_cast<std::streamsize>(got.size()));

	if (!in)
	{
		throw std::runtime_error("Failed to read expected number of bytes");
	}

	return got == payload;
}

int main()
{
	try
	{
		const char* path = "mapped_64k.bin";
		PrepareFileUsingMapping(path);
		if (VerifyFileContentWithIostream(path))
		{
			std::cout << "Payload was found\n";
		}
		else
		{
			std::cout << "Payload was not found\n";
		}
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << "\n";
		return EXIT_FAILURE;
	}
}
