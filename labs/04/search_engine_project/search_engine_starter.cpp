// Заготовка для лабораторной работы «Синхронизация между потоками».
//
// Эта программа намеренно ОДНОПОТОЧНАЯ и НЕ ЯВЛЯЕТСЯ ПОТОКОБЕЗОПАСНОЙ.
// Она умеет добавлять текстовые файлы и искать по их содержимому, однако
// алгоритм поиска специально упрощён. Студенты должны доработать программу.
//
// Сборка (GCC/Clang):
//   g++ -std=c++20 -O2 -Wall -Wextra -pedantic search_engine_starter.cpp -o search_engine
//
// Запуск:
//   ./search_engine
//
// Команды:
//   add_file PATH       Добавить текстовый файл или обновить ранее добавленный.
//   find WORDS...       Найти документы хотя бы с одним словом запроса.
//   help                Показать список команд.
//   exit                Завершить программу.
//
// TODO для лабораторной работы (пока НЕ реализовано):
//   * Сделать DocumentDb потокобезопасным; поддержать одновременное чтение и запись.
//   * Добавить команды индексирования каталогов и удаления документов.
//   * Разбивать текст на отдельные слова вместо поиска подстрок.
//   * Индексировать файлы в фоне с использованием ограниченного пула потоков.
//   * Необязательно: инвертированный индекс и ранжирование по TF-IDF.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace db
{

using DocumentId = std::uint64_t;

struct SearchResultItem
{
    DocumentId docId;
    fs::path docPath;
    double relevance;
};

using SearchResults = std::vector<SearchResultItem>;

// Приведение регистра работает только для ASCII, но НЕ для Unicode.
std::string ToLowerAscii(std::string text)
{
    for (char& ch : text)
    {
        if (ch >= 'A' && ch <= 'Z')
        {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return text;
}

// Упрощение: «словом» считается любая последовательность между пробелами.
// Знаки пунктуации пока не считаются границами слов.
std::vector<std::string> SplitQuery(std::string_view query)
{
    std::istringstream input{std::string(query)};
    std::vector<std::string> words;

    for (std::string word; input >> word;)
    {
        word = ToLowerAscii(std::move(word));
        // Повторяющиеся слова запроса не должны увеличивать релевантность документа.
        if (std::find(words.begin(), words.end(), word) == words.end())
        {
            words.push_back(std::move(word));
        }
    }
    return words;
}

class DocumentDb
{
public:
    // Добавляет документ или обновляет его содержимое без изменения идентификатора.
    // ВАЖНО: метод не потокобезопасен. Одновременные вызовы AddDocument/FindDocuments
    // необходимо корректно поддержать в рамках лабораторной работы.
    void AddDocument(const fs::path& path)
    {
        const fs::path normalizedPath = fs::canonical(path);
        if (!fs::is_regular_file(normalizedPath))
        {
            throw std::runtime_error("not a regular file: " + normalizedPath.string());
        }

        // Сначала читаем файл целиком и только потом изменяем базу документов.
        std::ifstream input(normalizedPath);
        if (!input)
        {
            throw std::runtime_error("cannot open file: " + normalizedPath.string());
        }

        std::vector<std::string> lines;
        for (std::string line; std::getline(input, line);)
        {
            lines.push_back(ToLowerAscii(std::move(line)));
        }
        if (!input.eof())
        {
            throw std::runtime_error("cannot read file: " + normalizedPath.string());
        }

        // TODO: обеспечить потокобезопасное добавление и обновление документов.
        auto it = std::find_if(m_documents.begin(), m_documents.end(),
            [&](const Document& doc) { return doc.path == normalizedPath; });

        if (it != m_documents.end())
        {
            it->lines = std::move(lines);  // Обновляем существующий документ.
        }
        else
        {
            m_documents.push_back(Document{
                m_nextDocumentId, normalizedPath, std::move(lines)
            });
            ++m_nextDocumentId;
        }
    }

    SearchResults FindDocuments(
        std::string queryString,
        std::size_t start = 0,
        std::size_t count = std::numeric_limits<std::size_t>::max()) const
    {
        const auto words = SplitQuery(queryString);
        SearchResults results;
        if (words.empty())
        {
            return results;
        }

        // TODO: обеспечить безопасный поиск при одновременном изменении базы.
        // TODO: искать отдельные слова, а не произвольные подстроки.
        for (const Document& doc : m_documents)
        {
            double score = 0;
            for (const std::string& word : words)
            {
                // Начисляем 1 балл, если слово запроса встречается где-либо в файле.
                // Например, запрос "art" найдёт также "start" (это намеренное упрощение).
                for (const std::string& line : doc.lines)
                {
                    if (line.find(word) != std::string::npos)
                    {
                        score += 1;
                        break;
                    }
                }
            }
            if (score > 0)
            {
                results.push_back({doc.id, doc.path, score});
            }
        }

        // Чем больше слов запроса найдено, тем выше релевантность.
        // При равной релевантности сортируем по ID для предсказуемого вывода.
        std::sort(results.begin(), results.end(),
            [](const SearchResultItem& a, const SearchResultItem& b)
            {
                if (a.relevance != b.relevance)
                {
                    return a.relevance > b.relevance;
                }
                return a.docId < b.docId;
            });

        if (start >= results.size())
        {
            return {};
        }
        const auto begin = results.begin() + static_cast<std::ptrdiff_t>(start);
        const auto remaining = results.size() - start;
        const auto take = std::min(count, remaining);
        SearchResults page;
        page.reserve(take);
        page.insert(page.end(), begin,
            begin + static_cast<std::ptrdiff_t>(take));
        return page;
    }

private:
    struct Document
    {
        DocumentId id;
        fs::path path;
        std::vector<std::string> lines; // Текст документа: одна строка вектора на строку файла.
    };

    std::vector<Document> m_documents;
    DocumentId m_nextDocumentId = 1;
};

} // пространство имён db

std::string Trim(std::string_view text)
{
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos)
    {
        return {};
    }
    const auto last = text.find_last_not_of(" \t\r\n");
    return std::string(text.substr(first, last - first + 1));
}

class UserInterface
{
public:
    explicit UserInterface(db::DocumentDb& documents)
        : m_documents(documents)
    {
    }

    void RunCommands()
    {
        PrintHelp();
        for (std::string line; std::getline(std::cin, line);)
        {
            std::istringstream input(line);
            std::string command;
            input >> command;
            if (command.empty())
            {
                continue;
            }

            std::string args;
            std::getline(input, args);
            args = Trim(args);

            try
            {
                if (command == "add_file")
                {
                    AddFile(args);
                }
                else if (command == "find")
                {
                    Find(args);
                }
                else if (command == "help")
                {
                    PrintHelp();
                }
                else if (command == "exit" || command == "quit")
                {
                    return;
                }
                else
                {
                    std::cout << "error: unknown command: " << command << '\n';
                }
            }
            catch (const std::exception& e)
            {
                std::cout << "error: " << e.what() << '\n';
            }
        }
    }

private:
    void AddFile(const std::string& path)
    {
        if (path.empty())
        {
            std::cout << "error: expected add_file PATH\n";
            return;
        }
        m_documents.AddDocument(path);
        std::cout << "Document indexed: " << fs::canonical(path).string() << '\n';
    }

    void Find(const std::string& query)
    {
        if (db::SplitQuery(query).empty())
        {
            std::cout << "error: empty query\n";
            return;
        }

        const auto results = m_documents.FindDocuments(query, 0, 10);
        std::cout << "Results (" << results.size() << "):\n";
        for (std::size_t i = 0; i < results.size(); ++i)
        {
            const auto& item = results[i];
            std::cout << (i + 1)
                      << ". id:" << item.docId
                      << ", relevance:" << std::fixed << std::setprecision(2)
                      << item.relevance
                      << ", path:" << item.docPath.string() << '\n';
        }
        if (results.empty())
        {
            std::cout << "No documents found.\n";
        }
    }

    static void PrintHelp()
    {
        std::cout << "Commands:\n"
                  << "  add_file PATH  - add/update a text file\n"
                  << "  find WORDS...  - search (up to 10 results)\n"
                  << "  help           - show these commands\n"
                  << "  exit           - quit\n";
    }

    db::DocumentDb& m_documents;
};

int main()
{
    db::DocumentDb documents;
    UserInterface ui(documents);
    ui.RunCommands();
}
