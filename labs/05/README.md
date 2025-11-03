# Лабораторная работа №5

- [Лабораторная работа №5](#лабораторная-работа-5)
  - [Задания](#задания)
    - [Требования](#требования)
    - [Задание №1 — Симулятор виртуальной памяти — 100 баллов](#задание-1--симулятор-виртуальной-памяти--100-баллов)
      - [Матрица проверки прав (упрощённая x86-семантика)](#матрица-проверки-прав-упрощённая-x86-семантика)
      - [R/M (Referenced/Modified): кто и когда ставит](#rm-referencedmodified-кто-и-когда-ставит)
      - [Повтор после Page Fault](#повтор-после-page-fault)
      - [Бонус за двухуровневую таблицу страниц — 30 баллов](#бонус-за-двухуровневую-таблицу-страниц--30-баллов)
    - [Задание №2 — Обработчик Page Fault и алгоритмы вытеснения страниц — 100 баллов](#задание-2--обработчик-page-fault-и-алгоритмы-вытеснения-страниц--100-баллов)
      - [Дано](#дано)
      - [Первоначальная инициализация таблицы страниц](#первоначальная-инициализация-таблицы-страниц)
      - [Обработчик Page Fault-ов](#обработчик-page-fault-ов)
      - [2) Реализуйте один из алгоритм вытеснения: Aging или WSClock](#2-реализуйте-один-из-алгоритм-вытеснения-aging-или-wsclock)
      - [3) Подсчёт Page Fault-ов и «идеального» числа (OPT)](#3-подсчёт-page-fault-ов-и-идеального-числа-opt)
      - [Протокол «повтор после Page Fault»](#протокол-повтор-после-page-fault)
      - [Своп-подсистема (упрощённая)](#своп-подсистема-упрощённая)
      - [Безопасность: «page fault в обработчике page fault»](#безопасность-page-fault-в-обработчике-page-fault)
      - [Учёт и отчёт](#учёт-и-отчёт)
      - [Тестирование](#тестирование)
      - [Бонус в 30 баллов за двухуровневую таблицу страниц](#бонус-в-30-баллов-за-двухуровневую-таблицу-страниц)
    - [Задание 3 — Менеджер памяти — 120 баллов](#задание-3--менеджер-памяти--120-баллов)
      - [Бонус: сделать менеджер памяти потокобезопасным — 10 баллов](#бонус-сделать-менеджер-памяти-потокобезопасным--10-баллов)

## Задания

- Для получения оценки "удовлетворительно" нужно набрать не менее 120 баллов.
- На оценку хорошо нужно набрать не менее 240 баллов.
- Для получения оценки "отлично" нужно набрать не менее 360 баллов.

### Требования

Обязательно проверяйте успешность всех вызовов функций операционной системы и не оставляйте ошибки незамеченными.

Ваш код должен иметь уровень безопасности исключений не ниже базового.
Для этого разработайте (или возьмите готовую) RAII-обёртку, автоматизирующую
управление ресурсами операционной системы.

### Задание №1 — Симулятор виртуальной памяти — 100 баллов

Разработайте класс `PhysicalMemory`, моделирующих работу физической памяти,
и класс `VirtualMemory`, который производит трансляцию виртуальных адресов в физические по таблице страниц подобно тому,
как это происходит в процессорах x86.
Страницы в таблицах должны поддерживать биты A (Accessed aka Referenced),
D (Dirty aka Modified), P (Present), US (User/Supervisor), RW (Read/Write)
а также биты защиты: R/W, Execute, Supervisor.

```txt
 31                        12 11 10  9  8  7  6  5  4  3  2  1  0
+--+-------------------------+--+--+--+--+--+--+--+--+--+--+--+--+
|NX frame number (20 bits)   |  |  |  |  |  |D |A |  |  |US|RW|P |
+--+-------------------------+--+--+--+--+--+--+--+--+--+--+--+--+
```

- P (Present)
- RW (Read/Write)
- US (User/Supervisor)
- A (Accessed/Referenced) — можно использовать как R
- D (Dirty/Modified) — как M
- NX (No-Execute) — в x86 PAE/NX существует, для учебных целей можно эмулировать как старший бит
  номера фрейма, что предполагает, что неисполняемые страницы должны находиться в верхних
  2Гб физической памяти.

```cpp
struct PhysicalMemoryConfig
{
  uint32_t numFrames = 1024;
  uint32_t frameSize = 4096;
};

// Представляет абстракцию физической памяти
class PhysicalMemory
{
public:
  explicit PhysicalMemory(PhysicalMemoryConfig cfg);

  uint32_t GetSize() const noexcept;

  // Операции чтения. Попытка прочитать данные по не адресу, не кратному размеру соответствующего типа,
  // либо по адресу за пределами физической памяти приводит к выбрасыванию исключения.
  uint8_t Read8(uint32_t address) const;
  uint16_t Read16(uint32_t address) const;
  uint32_t Read32(uint32_t address) const;
  uint64_t Read64(uint32_t address) const;

  // Операции записи. Попытка записать данные по не адресу, не кратному размеру соответствующего типа,
  // либо по адресу за пределами физической памяти приводит к выбрасыванию исключения.
  void Write8(uint32_t address, uint8_t value);
  void Write16(uint32_t address, uint16_t value);
  void Write32(uint32_t address, uint32_t value);
  void Write64(uint32_t address, uint64_t value);
};

enum class Access
{
  Read,
  Write,
};

enum class PageFaultReason {
  NotPresent,
  WriteToReadOnly,
  ExecOnNX,
  UserAccessToSupervisor,
  PhysicalAccessOutOfRange,
  MisalignedAccess,
};
 
class OSHandler
{
public:
  virtual ~OSHandler() = default;
  virtual bool OnPageFault(VirtualMemory& vm, uint32 virtualPageNumber,
                           Access access, PageFaultReason reason) = 0;
  // Разместите здесь прочие методы-обработчики, если нужно
};

// 4-КиБ страницы, 32-битный PTE (учебная модель)
struct PTE
{
  uint32_t raw = 0;

  static constexpr uint32_t P   = 1u << 0;   // Present
  static constexpr uint32_t RW  = 1u << 1;   // 1 = Read/Write, 0 = Read-Only
  static constexpr uint32_t US  = 1u << 2;   // 1 = User, 0 = Supervisor
  static constexpr uint32_t A   = 1u << 5;   // Accessed (используем как R)
  static constexpr uint32_t D   = 1u << 6;   // Dirty (используем как M)
  static constexpr uint32_t NX  = 1u << 31;  // No-Execute (учебное поле)

  static constexpr uint32_t FRAME_SHIFT = 12;
  static constexpr uint32_t FRAME_MASK  = 0xFFFFF000u;

  uint32_t GetFrame() const { return (raw & FRAME_MASK) >> FRAME_SHIFT; }
  void     SetFrame(uint32_t fn) { raw = (raw & ~FRAME_MASK) | (fn << FRAME_SHIFT); }

  bool IsPresent() const { return raw & P;  }
  void SetPresent(bool v) { raw = v ? (raw | P)  : (raw & ~P); }

  bool IsWritable() const { return raw & RW; }
  void SetWritable(bool v) { raw = v ? (raw | RW) : (raw & ~RW); }

  bool IsUser() const { return raw & US; }
  void SetUser(bool v) { raw = v ? (raw | US) : (raw & ~US); }

  bool IsAccessed() const { return raw & A;  }
  void SetAccessed(bool v) { raw = v ? (raw | A)  : (raw & ~A); }

  bool IsDirty() const { return raw & D;  }
  void SetDirty(bool v) { raw = v ? (raw | D)  : (raw & ~D); }

  bool IsNX() const { return raw & NX; }
  void SetNX(bool v) { raw = v ? (raw | NX) : (raw & ~NX); }
};

enum class Privilege{User, Supervisor};

class VirtualMemory
{
public:
  explicit VirtualMemory(PhysicalMemory& physicalMem, OSHandler& handler);

  // Задать физический адрес таблицы страниц (должен быть кратен 4096 байтам).
  void SetPageTableAddress(uint32_t physicalAddress);
  [[nodiscard]] uint32_t GetPageTableAddress() const noexcept;

  // Операции чтения. Может вызвать обработчик ОС в следующих ситуациях:
  // чтение невыровненных данных, физическая страница отсутствует, нарушение прав доступа и т.п.
  [[nodiscard]] uint8_t Read8(uint32_t address, Privilege privilege, bool execute = false) const;
  [[nodiscard]] uint16_t Read16(uint32_t address, Privilege privilege, bool execute = false) const;
  [[nodiscard]] uint32_t Read32(uint32_t address, Privilege privilege, bool execute = false) const;
  [[nodiscard]] uint64_t Read64(uint32_t address, Privilege privilege, bool execute = false) const;

  // Операции записи. Может вызвать обработчик ОС в следующих ситуациях:
  // запись невыровненных данных, физическая страница отсутствует, нарушение прав доступа и т.п.
  void Write8(uint32_t address, uint8_t value, Privilege privilege);
  void Write16(uint32_t address, uint16_t value, Privilege privilege);
  void Write32(uint32_t address, uint32_t value, Privilege privilege);
  void Write64(uint32_t address, uint64_t value, Privilege privilege);
};
```

#### Матрица проверки прав (упрощённая x86-семантика)

| Параметры вызова | Проверки PTE | Результат |
|---|---|---|
| `Read*` (`execute=false`, `Privilege::User`) | `P=1`, `US=User` (или `US=Supervisor` запрещён), NX игнорируется | Успех → R=1 |
| `Read*` (`execute=false`, `Privilege::Supervisor`)  | `P=1` (Supervisor доступен), NX игнорируется | Успех → R=1 |
| `Read*` (`execute=true`, `Privilege::Supervisor`)  | `P=1`, `US=User`, **NX=0** | Успех → R=1 |
| `Read*` (`execute=true`, `Privilege::Supervisor`)   | `P=1`, **NX=0** | Успех → R=1 |
| `Write*` (`Privilege::User`) | `P=1`, `US=User`, **RW=1**, NX игнорируется | Успех → R=1, M=1 |
| `Write*` (`Privilege::Supervisor`)  | `P=1`, **RW=1** | Успех → R=1, M=1 |

Примечания:

- «User не может в Supervisor-страницы»: если `Privilege::User`, то доступ к странице с `US=Supervisor` вызывает Page Fault.
- `execute=true` проверяет **только** NX (и, конечно, `P`/`US`); чтение отдельно не проверяется (Execute ⇒ Read).
- Запись требует **RW=1**; чтение отдельно не проверяется (Write ⇒ Read).

#### R/M (Referenced/Modified): кто и когда ставит

- Любой успешный доступ (и чтение, и запись, включая «исполнение» с execute=true)
 должен поставить бит R=1 у соответствующего PTE.
- Любая успешная запись (любой Write*) должна поставить бит M=1 у соответствующего PTE.
- Обновление R/M выполняется записью соответствующего PTE (через PhysicalMemory::Write*)
  — т.е. в «хранилище» физической памяти.

#### Повтор после Page Fault

После вызова OnPageFault(...) реализация VirtualMemory обязана повторить перевод адреса и попытку доступа.
Если новый PTE по-прежнему P=0 или нарушает права, операция завершается ошибкой.

Классы `PhysicalMemory` и `VirtualMemory` должны быть покрыты юнит-тестами.

#### Бонус за двухуровневую таблицу страниц — 30 баллов

Бонус начисляется за поддержку двухуровневых таблиц страниц, подобно тому, как это реализовано в архитектуре x86.

Для 32-бит VA/4К страница стандартно:

- 10 бит (22..31 биты) — Page Directory Index (PDI),
- 10 бит (12..21 биты) — Page Table Index (PTI),
- 12 бит (0..11 биты) — Offset.

Метод SetPageTableAddress устанавливает физический адрес Page Directory.

- Каждая запись в таблице Page Directory (PDE) хранит физический адрес Page Table.
- Каждая запись в этой таблице Page Table (PTE) хранит frame + флаги.

Все 3 узла (PDE, PTE и флаги PDE) должны проверяться на P=1 и права доступа, а биты A/D обновляться как на PTE так и PDE.

### Задание №2 — Обработчик Page Fault и алгоритмы вытеснения страниц — 100 баллов

- Реализовать обработчик прерывания страницы (Page Fault) поверх уже сделанного вами симулятора `VirtualMemory`/`PhysicalMemory`.
- Внедрить **практически применимый алгоритм замещения** (на выбор: **Aging** или **WSClock**).
- Параллельно **собирать строку обращений** к страницам (reference string)
  и **подсчитывать оптимальное число промахов** по алгоритму Бэлайда (**OPT/MIN**) для сравнения.

#### Дано

- `PhysicalMemory` (физическая память, 4 КиБ страницы).
- `VirtualMemory` (трансляция VA→PA по таблицам страниц; флаги `P/R/W/US/NX`, биты `R/M`; вызов `OSHandler::OnPageFault`).
- Контракт `OSHandler` – вы реализуете `class MyOS : public OSHandler`.

Рекомендую воспользоваться вспомогательными классами VMContext, VMUserContext и VMSupervisorContext,
чтобы не передавать режим User/Supervisor при каждом доступе к виртуальной памяти.

```cpp
class VMContext
{
public:
  [[nodiscard]] uint8_t Read8(uint32_t address, bool execute = false) const;
  [[nodiscard]] uint16_t Read16(uint32_t address, bool execute = false) const;
  [[nodiscard]] uint32_t Read32(uint32_t address, bool execute = false) const;
  [[nodiscard]] uint64_t Read64(uint32_t address, bool execute = false) const;
  void Write8(uint32_t address, uint8_t value);
  void Write16(uint32_t address, uint16_t value);
  void Write32(uint32_t address, uint32_t value);
  void Write64(uint32_t address, uint64_t value);;
protected:
  explicit VMContext(VirtualMemory& vm, Privilege privilege);
  VirtualMemory& GetVM() const noexcept;
private:
  VirtualMemory* m_vm;
  Privilege m_privilege;
};

class VMUserContext : public VMContext
{
public:
  explicit VMUserContext(VirtualMemory& vm): VMContext{ vm, Privilege::User }{}
};

class VMSupervisorContext : public VMContext
{
public:
  explicit VMUserContext(VirtualMemory& vm): VMContext{ vm, Privilege::Supervisor }{}
  void SetPageTableAddress(uint32_t physicalAddress);
  uint32_t GetPageTableAddress() const noexcept;
};
```

#### Первоначальная инициализация таблицы страниц

- Ваш менеджер памяти должен сперва проинициализировать таблицу страниц физической памяти,
  а затем все дальнейшие операции над ней выполнять только через виртуальную память.
- Страницы виртуальной памяти, в которых располагаются таблицы страниц,
  должны быть доступны только в режиме супервизора.

#### Обработчик Page Fault-ов

Реализовать `MyOS::OnPageFault(VirtualMemory& vm, uint32_t vpn, Access access, PageFaultReason reason)`:

- **Классификация причины:** P=0 (страница не в памяти)
  / нарушение прав (RW/NX/US) / невыровненный доступ (по правилам задания №1).
- **Стратегия подкачки:**
  - **Local replacement** (вытесняем в пределах «рабочего набора» текущего процесса/адресного пространства).
  - Если свободный фрейм есть — загрузить страницу в него.
  - Если нет — выбрать жертву по выбранному алгоритму (в зависимости от выбранного варианта).
- **Запись в своп:**
  - Если жертва `M=1` — записать в «swap-файл» и проставить «номер слота».
  - Если `M=0` — запись не нужна.
- **Загрузка из свопа/с диска:** при P=0:
  - Если страница была ранее выгружена — прочесть из своп-слота.
  - Если страница «никогда не загружалась» — инициализировать нулями.
- **Обновление PTE:**
  - Установить `P=1`, сбросить `R=0`, `M=0`.
  - Корректно проставить права (RW/US/NX) по политике ОС.
  - (В модели `VirtualMemory` допускается менять PTE через `vm`/`physical` с `isSupervisor=true`.)
- **Повтор операции:** после возврата из `OnPageFault` попытка доступа **повторяется** (см. протокол ниже).

#### 2) Реализуйте один из алгоритм вытеснения: Aging или WSClock

#### 3) Подсчёт Page Fault-ов и «идеального» числа (OPT)

- Во время работы `VirtualMemory` **собирайте строку обращений** к виртуальным страницам: `(vpn, access, t)`.
- Напишите офлайн-процедуру **моделирования OPT** (Бэлайда/MIN):
  - На фиксированном числе фреймов, при известной полной строке обращений,
    при промахе вытесняем страницу,
    чьё **следующее использование дальше всего в будущем** (или никогда).
- Сравнить: фактические PF вашего алгоритма vs PF(OPT) на том же reference string.

#### Протокол «повтор после Page Fault»

- После вызова `OnPageFault(vm, vpn, access, reason)` реализация `VirtualMemory` **обязана повторить** перевод и попытку доступа.
- Повтор считается успешным, если теперь `P=1` и права удовлетворены (`US/RW/NX`).
- Иначе — операция завершится ошибкой/исключением уровня VM.

#### Своп-подсистема (упрощённая)

Для хранения страниц реализуйте класс `SwapManager`:

```cpp
class SwapManager
{
public:
  explicit SwapManager(VMSupervisorContext& vmContext, const std::filesystem::path& path);
  uint32_t AllocateSlot(uint32_t virtualPageNumber);
  void FreeSlot(uint32_t virtualPageNumber);
  // Считывает страницу из swap-файла 
  void ReadPage(uint32_t slot, uint32_t virtualPageNumber);
  // Записывает страницу в файл
  void WritePage(uint32_t slot, uint32_t virtualPageNumber);
};
```

#### Безопасность: «page fault в обработчике page fault»

- В `VirtualMemory` предусмотрите **детектор ре-ентрантного fault**:
  если в момент `OnPageFault` обработчик обращается к странице `P=0` и возникает **второй** page fault,
  отметьте это как **фатальную ошибку ядра** (например, `KernelPanic`/`DoubleFault`).
- В MyOS пре

#### Учёт и отчёт

- Добавьте счётчики:
  - Общее число page faults.
  - Число записей в своп / чтений из свопа.
  - Ретавлидные метрики (например, процент dirty-эвиктов).
- Сравнение: PF(ваш алгоритм) vs PF(OPT) (на тех же входных данных).

#### Тестирование

Для класса  `MyOS` (и, если понадобится, для других) напишите юнит-тесты.

#### Бонус в 30 баллов за двухуровневую таблицу страниц

Бонус начисляется за поддержку "обработчиком ОС" двухуровневых таблиц страниц,
подобно тому, как это реализовано в архитектуре x86.

Для 32-бит VA/4К страница стандартно:

- 10 бит (22..31 биты) — Page Directory Index (PDI),
- 10 бит (12..21 биты) — Page Table Index (PTI),
- 12 бит (0..11 биты) — Offset.

Метод SetPageTableAddress устанавливает физический адрес Page Directory.

- Каждая запись в таблице Page Directory (PDE) хранит физический адрес Page Table.
- Каждая запись в этой таблице Page Table (PTE) хранит frame + флаги.

Все 3 узла (PDE, PTE и флаги PDE) должны проверяться на P=1 и права доступа, а биты A/D обновляться как на PTE так и PDE.

### Задание 3 — Менеджер памяти — 120 баллов

Разработайте класс `MemoryManager`, позволяющий динамически выделять и освобождать память
внутри блока памяти, переданного ему в конструктор.

Для класса разработайте необходимый набор юнит-тестов.

```c++
#include <cassert>
#include <memory>
#include <utility>

class MemoryManager
{
public:
    // Инициализирует менеджер памяти непрерывным блоком size байт,
    // начиная с адреса start.
    // Возвращает true в случае успеха и false в случае ошибки
    // Методы Allocate и Free должны работать с этим блоком памяти для хранения данных.
    // Указатель start должен быть выровнен по адресу, кратному sizeof(std::max_align_t)
    MemoryManager(void* start, size_t size) noexcept
    {
    }

    MemoryManager(const MemoryManager&) = delete;
    MemoryManager& operator=(const MemoryManager&) = delete;

    // Выделяет блок памяти внутри размером size байт и возвращает адрес выделенного
    // блока памяти. Возвращённый указатель должен быть выровнен по адресу, кратному align.
    // Параметр align должен быть степенью числа 2.
    // В случае ошибки (нехватка памяти, невалидные параметры) возвращает nullptr.
    // Полученный таким образом блок памяти должен быть позднее освобождён методом Free
    void* Allocate(size_t size, size_t align = sizeof(std::max_align_t)) noexcept
    {
        return nullptr;
    }

    // Освобождает область памяти, ранее выделенную методом Allocate,
    // делая её пригодной для повторного использования. После этого указатель
    // перестаёт быть валидным.
    // Если addr — нулевой указатель, метод не делает ничего
    // Если addr — не является валидным указателем, возвращённым ранее методом Allocate,
    // поведение метода не определено.
    void Free(void* addr) noexcept
    {
    }
}; 

int main()
{
    alignas(sizeof(max_align_t)) char buffer[1000];

    MemoryManager memoryManager(buffer, std::size(buffer));

    auto ptr = memoryManager.Allocate(sizeof(double));

    auto value = std::construct_at(static_cast<double*>(ptr), 3.1415927);
    assert(*value == 3.1415927);
        
    memoryManager.Free(ptr);
}
```

Класс должен использовать O(1) памяти в дополнение к переданному в конструктор блоку памяти.
При освобождении блока, который соседствует с одним или двумя свободными блоками,
должно происходить объединение пустых блоков в один.

В качестве идеи для реализации можете воспользоваться следующей структурой данных.

![memory](images/memory-manager.svg)

Подсказка: при размещении заголовков блоков в памяти учитывайте, что они должны быть выровнены по адресу, кратному `alignof(ТипЗаголовка)`.
Чтобы по адресу освобождаемого блока памяти можно было определить адрес его заголовка,
заголовок должен располагаться перед блоком данных.
Поэтому блок памяти следует фактически располагать по адресу, кратному `max(align, alignof(ТипЗаголовка))`.
Это может потребовать скорректировать указатель и размер в предыдущем блоке.

#### Бонус: сделать менеджер памяти потокобезопасным — 10 баллов

Методы класса `MemoryManager` должно быть возможно безопасно вызывать из нескольких потоков одновременно.
Разработайте модульные тесты для проверки этого функционала.
