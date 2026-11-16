#pragma once

// Публичный бинарный контракт библиотеки. Между модулями передаём только
// простые C-совместимые типы. Строки — нуль-терминированные байты UTF-8.
#if defined(_WIN32)
#if defined(STRING_LIBRARY_BUILD)
#define STRING_API __declspec(dllexport)
#else
#define STRING_API
#endif
#define STRING_CALL __cdecl
#elif defined(__GNUC__) || defined(__clang__)
#define STRING_API __attribute__((visibility("default")))
#define STRING_CALL
#else
#define STRING_API
#define STRING_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Создаёт динамическую копию text. Результат принадлежит вызывающей стороне,
// но освобождать его МОЖНО ТОЛЬКО вызовом FreeString из этой же библиотеки.
// Возвращает nullptr при ошибке выделения памяти или при text == nullptr.
STRING_API char* STRING_CALL MakeString(const char* text);

// Освобождает строку, ранее полученную из MakeString; допускает nullptr.
STRING_API void STRING_CALL FreeString(char* text);

#ifdef __cplusplus
}

// Типы указателей для вызова экспортируемых функций из C++ приложения.
// Соглашение о вызовах должно совпадать с определениями в библиотеке.
using MakeStringFn = char*(STRING_CALL*)(const char*);
using FreeStringFn = void(STRING_CALL*)(char*);
#endif
