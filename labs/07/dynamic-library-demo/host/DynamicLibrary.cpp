#include "DynamicLibrary.h"

#include <stdexcept>
#include <string>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

DynamicLibrary::DynamicLibrary(const std::filesystem::path& path)
{
#ifdef _WIN32
	// Используем Unicode-вариант WinAPI, чтобы путь мог содержать кириллицу.
	HMODULE module = LoadLibraryW(path.c_str());
	if (module == nullptr)
	{
		throw std::system_error(static_cast<int>(GetLastError()),
			std::system_category(), "LoadLibraryW");
	}
	m_handle = module;
#else
	m_handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
	if (m_handle == nullptr)
	{
		const char* error = dlerror();
		throw std::runtime_error(std::string("dlopen: ") + (error ? error : "неизвестная ошибка"));
	}
#endif
}

DynamicLibrary::~DynamicLibrary()
{
	if (m_handle == nullptr)
	{
		return;
	}
#ifdef _WIN32
	FreeLibrary(static_cast<HMODULE>(m_handle));
#else
	dlclose(m_handle);
#endif
}

void* DynamicLibrary::FindSymbol(const char* name) const
{
#ifdef _WIN32
	FARPROC symbol = GetProcAddress(static_cast<HMODULE>(m_handle), name);
	if (symbol == nullptr)
	{
		throw std::system_error(static_cast<int>(GetLastError()),
			std::system_category(),
			std::string("GetProcAddress(") + name + ")");
	}
	// WinAPI гарантирует получение адреса экспортируемой функции.
	return reinterpret_cast<void*>(symbol);
#else
	// dlsym может законно вернуть nullptr для символа с нулевым значением,
	// поэтому отсутствие символа проверяем через dlerror().
	dlerror();
	void* symbol = dlsym(m_handle, name);
	if (const char* error = dlerror())
	{
		throw std::runtime_error(std::string("dlsym(") + name + "): " + error);
	}
	return symbol;
#endif
}
