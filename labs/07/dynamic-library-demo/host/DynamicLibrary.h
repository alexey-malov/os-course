#pragma once

#include <filesystem>
#include <string>
#include <type_traits>

// RAII-обёртка над LoadLibrary / FreeLibrary (Windows)
// и dlopen / dlclose (Linux).
class DynamicLibrary final
{
public:
	explicit DynamicLibrary(const std::filesystem::path& path);
	~DynamicLibrary();

	DynamicLibrary(const DynamicLibrary&) = delete;
	DynamicLibrary& operator=(const DynamicLibrary&) = delete;
	DynamicLibrary(DynamicLibrary&&) = delete;
	DynamicLibrary& operator=(DynamicLibrary&&) = delete;

	// Ищет экспортируемую функцию по имени. Если символ не найден,
	// выбрасывает исключение с описанием ошибки.
	template <typename FunctionPointer>
	FunctionPointer GetFunction(const char* name) const
	{
		static_assert(std::is_pointer_v<FunctionPointer> && std::is_function_v<std::remove_pointer_t<FunctionPointer>>,
			"GetFunction expects a function-pointer type");
		return reinterpret_cast<FunctionPointer>(FindSymbol(name));
	}

private:
	void* FindSymbol(const char* name) const;

	void* m_handle = nullptr;
};
