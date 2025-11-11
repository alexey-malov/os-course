#ifdef _WIN32
#include "../lib/scope_guards.h"
#include <cstring> // std::memcpy
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <windows.h>
#else
#error This example is for Windows only
#endif

namespace
{
constexpr std::size_t FileSize = 64 * 1024; // 64 KiB
constexpr std::size_t Offset = 16 * 1024; // 16 KiB
const std::string payload = "MMAP_WAS_HERE";

// Опционально: удобный преобразователь GetLastError() => std::string
std::string WinErr(DWORD code = GetLastError())
{
	char* msg = nullptr;

	DWORD len = FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPSTR)&msg, 0, nullptr);

	auto closeMsg = sg::MakeScopeExit([msg]() noexcept {
		if (msg)
			LocalFree(msg);
	});

	return (len && msg) ? std::string(msg, msg + len) : "Unknown error";
}

void PrepareFileUsingMapping(const char* path)
{
	// 1) Создаём/обнуляем файл нужного размера (64 KiB)
	HANDLE hFile = CreateFileA(
		path,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ, // позволим параллельное чтение
		nullptr,
		CREATE_ALWAYS, // заново создаём/обнуляем
		FILE_ATTRIBUTE_NORMAL,
		nullptr);
	if (hFile == INVALID_HANDLE_VALUE)
		throw std::runtime_error(std::string("CreateFileA failed: ") + WinErr());

	// scope guard для CloseHandle(hFile)
	auto closeFile = sg::MakeScopeExit([hFile]() noexcept { CloseHandle(hFile); });

	// Устанавливаем размер файла = FileSize
	LARGE_INTEGER liSize;
	liSize.QuadPart = static_cast<LONGLONG>(FileSize);
	if (!SetFilePointerEx(hFile, liSize, nullptr, FILE_BEGIN))
		throw std::runtime_error(std::string("SetFilePointerEx failed: ") + WinErr());
	if (!SetEndOfFile(hFile))
		throw std::runtime_error(std::string("SetEndOfFile failed: ") + WinErr());

	// 2) Создаём объект отображения файла
	HANDLE hMap = CreateFileMappingA(
		hFile,
		nullptr,
		PAGE_READWRITE,
		0, 0, // размер 0 => «весь файл»
		nullptr);
	if (!hMap)
		throw std::runtime_error(std::string("CreateFileMappingA failed: ") + WinErr());

	// scope guard для CloseHandle(hMap)
	auto closeMap = sg::MakeScopeExit([hMap]() noexcept { CloseHandle(hMap); });

	// 3) Отображаем весь файл на чтение и запись
	void* addr = MapViewOfFile(
		hMap,
		FILE_MAP_ALL_ACCESS,
		0, 0, // смещение = 0
		FileSize); // размер отображения (можно 0 = весь файл)
	if (!addr)
		throw std::runtime_error(std::string("MapViewOfFile failed: ") + WinErr());

	// scope guard для UnmapViewOfFile
	auto unmapView = sg::MakeScopeExit([addr]() noexcept {
		if (!UnmapViewOfFile(addr))
		{
			// Увы, здесь нельзя бросать исключения — понять, простить...
		}
	});

	// 4) Записываем данные начиная со смещения 16 KiB
	char* base = static_cast<char*>(addr);
	std::memcpy(base + Offset, payload.data(), payload.size());

	// 5) Синхронизируем изменённый диапазон с файлом
	if (!FlushViewOfFile(base + Offset, payload.size()))
		throw std::runtime_error(std::string("FlushViewOfFile failed: ") + WinErr());

	// (необязательно, но можно форсировать сброс метаданных файла)
	// FlushFileBuffers(hFile);}
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

} // namespace

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
