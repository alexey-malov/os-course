#include "DynamicLibrary.h"
#include "LibraryString.h"

#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace
{

std::filesystem::path ExecutableDirectory()
{
#ifdef _WIN32
	// Не используем текущий рабочий каталог: приложение можно запускать
	// из любой директории.
	std::wstring buffer(260, L'\0');
	for (;;)
	{
		const DWORD size = GetModuleFileNameW(
			nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
		if (size == 0)
		{
			throw std::system_error(static_cast<int>(GetLastError()),
				std::system_category(), "GetModuleFileNameW");
		}
		if (size < buffer.size())
		{
			buffer.resize(size);
			return std::filesystem::path(buffer).parent_path();
		}
		buffer.resize(buffer.size() * 2);
	}
#else
	// В Linux /proc/self/exe указывает на запущенный исполняемый файл.
	return std::filesystem::read_symlink("/proc/self/exe").parent_path();
#endif
}

std::filesystem::path PluginPath()
{
#ifdef _WIN32
	constexpr const char* filename = "string_plugin.dll";
#else
	constexpr const char* filename = "libstring_plugin.so";
#endif
	return ExecutableDirectory() / "plugins" / filename;
}

} // namespace

int main()
{
	try
	{
		const auto path = PluginPath();
		std::cout << "Loading: " << path.string() << '\n';

		auto library = std::make_shared<DynamicLibrary>(path);

		// При создании ищутся MakeString и FreeString. После этого
		// выделенная в плагине строка принадлежит RAII-обёртке.
		auto text = LibraryString::Make(
			library, "Hello from the dynamic library demo!");

		// Демонстрируем владение: основной код может отпустить библиотеку,
		// но её выгрузка отложится до уничтожения объекта text.
		library.reset();

		std::cout << text.ToString() << '\n';
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << "Error: " << error.what() << '\n';
		return 1;
	}
}
