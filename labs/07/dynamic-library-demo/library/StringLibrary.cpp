#include "StringApi.h"

#include <cstdlib>
#include <cstring>

extern "C" STRING_API char* STRING_CALL MakeString(const char* text)
{
	if (text == nullptr)
	{
		return nullptr;
	}

	const std::size_t length = std::strlen(text);
	auto* result = static_cast<char*>(std::malloc(length + 1));
	if (result == nullptr)
	{
		return nullptr;
	}

	std::memcpy(result, text, length + 1);
	return result;
}

extern "C" STRING_API void STRING_CALL FreeString(char* text)
{
	std::free(text);
}
