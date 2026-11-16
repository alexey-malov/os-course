#pragma once

#include "StringApi.h"

#include <memory>
#include <string>

class DynamicLibrary;

// RAII-обёртка для C-строки, выделенной в динамической библиотеке.
// Имя FreeString скрыто внутри класса: пользователь не передаёт deleter.
class LibraryString final
{
public:
	// СНАЧАЛА ищет MakeString и FreeString, ПОТОМ создаёт строку.
	// Иначе ошибка поиска FreeString могла бы вызвать утечку памяти.
	static LibraryString Make(std::shared_ptr<DynamicLibrary> library,
		const char* text);

	LibraryString(const LibraryString&) = delete;
	LibraryString& operator=(const LibraryString&) = delete;

	LibraryString(LibraryString&& other) noexcept = default;
	LibraryString& operator=(LibraryString&& other) noexcept;

	const char* c_str() const noexcept;
	std::string ToString() const;

private:
	LibraryString(std::shared_ptr<DynamicLibrary> library,
		char* text,
		FreeStringFn freeString);

	// Важно: поля уничтожаются в обратном порядке объявления.
	// Сначала data_ вызывает FreeString, только потом library_ позволяет
	// выгрузить библиотеку. Этим исключаем вызов кода выгруженной DLL/.so.
	std::shared_ptr<DynamicLibrary> m_library;
	std::unique_ptr<char, FreeStringFn> m_data;
};
