#include "LibraryString.h"

#include "DynamicLibrary.h"

#include <new>
#include <stdexcept>
#include <utility>

LibraryString::LibraryString(std::shared_ptr<DynamicLibrary> library,
	char* text,
	FreeStringFn freeString)
	: m_library(std::move(library))
	, m_data(text, freeString)
{
}

LibraryString LibraryString::Make(std::shared_ptr<DynamicLibrary> library,
	const char* text)
{
	if (!library || text == nullptr)
	{
		throw std::invalid_argument("LibraryString::Make: invalid argument");
	}

	// Оба символа ищутся до выделения памяти в библиотеке.
	auto freeString = library->GetFunction<FreeStringFn>("FreeString");
	auto makeString = library->GetFunction<MakeStringFn>("MakeString");

	char* result = makeString(text);
	if (result == nullptr)
	{
		throw std::bad_alloc();
	}

	return LibraryString(std::move(library), result, freeString);
}

LibraryString& LibraryString::operator=(LibraryString&& other) noexcept
{
	if (this != &other)
	{
		// Сначала освобождаем СВОЮ прежнюю строку через прежнюю библиотеку.
		// Лишь затем уменьшаем счётчик владения прежней DLL/.so.
		m_data.reset();
		m_library = std::move(other.m_library);
		m_data = std::move(other.m_data);
	}
	return *this;
}

const char* LibraryString::c_str() const noexcept
{
	return m_data ? m_data.get() : "";
}

std::string LibraryString::ToString() const
{
	return std::string(c_str());
}
