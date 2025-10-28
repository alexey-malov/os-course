# Лабораторная работа №5

- [Лабораторная работа №5](#лабораторная-работа-5)
  - [Задания](#задания)
    - [Требования](#требования)
    - [Задание №1 — Симулятор виртуальной памяти — 100 баллов](#задание-1--симулятор-виртуальной-памяти--100-баллов)
      - [Матрица проверки прав (упрощённая x86-семантика)](#матрица-проверки-прав-упрощённая-x86-семантика)
      - [R/M (Referenced/Modified): кто и когда ставит](#rm-referencedmodified-кто-и-когда-ставит)
      - [Повтор после Page Fault](#повтор-после-page-fault)
      - [Бонус за двухуровневую таблицу страниц — 30 баллов](#бонус-за-двухуровневую-таблицу-страниц--30-баллов)

## Задания

- Для получения оценки "удовлетворительно" нужно набрать не менее X баллов.
- На оценку хорошо нужно набрать не менее Y баллов.
- Для получения оценки "отлично" нужно набрать не менее Z баллов.

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
                           Access access, PageFaultReason reason, ) = 0;
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

class VirtualMemory
{
public:
  explicit VirtualMemory(PhysicalMemory& physicalMem, OSHandler& handler);

  // Задать физический адрес таблицы страниц (должен быть кратен 4096 байтам).
  void SetPageTableAddress(uint32_t physicalAddress);

  // Операции чтения. Может вызвать обработчик ОС в следующих ситуациях:
  // чтение невыровненных данных, физическая страница отсутствует, нарушение прав доступа и т.п.
  uint8_t Read8(uint32_t address, bool execute = false, bool isSupervisor = false) const;
  uint16_t Read16(uint32_t address, bool execute = false, bool isSupervisor = false) const;
  uint32_t Read32(uint32_t address, bool execute = false, bool isSupervisor = false) const;
  uint64_t Read64(uint32_t address, bool execute = false, bool isSupervisor = false) const;

  // Операции записи. Может вызвать обработчик ОС в следующих ситуациях:
  // запись невыровненных данных, физическая страница отсутствует, нарушение прав доступа и т.п.
  void Write8(uint32_t address, uint8_t value, bool isSupervisor = false);
  void Write16(uint32_t address, uint16_t value, bool isSupervisor = false);
  void Write32(uint32_t address, uint32_t value, bool isSupervisor = false);
  void Write64(uint32_t address, uint64_t value, bool isSupervisor = false);
private:
};
```

#### Матрица проверки прав (упрощённая x86-семантика)

| Параметры вызова | Проверки PTE | Результат |
|---|---|---|
| `Read*` (`execute=false`, `isSupervisor=false`) | `P=1`, `US=User` (или `US=Supervisor` запрещён), NX игнорируется | Успех → R=1 |
| `Read*` (`execute=false`, `isSupervisor=true`)  | `P=1` (Supervisor доступен), NX игнорируется | Успех → R=1 |
| `Read*` (`execute=true`, `isSupervisor=false`)  | `P=1`, `US=User`, **NX=0** | Успех → R=1 |
| `Read*` (`execute=true`, `isSupervisor=true`)   | `P=1`, **NX=0** | Успех → R=1 |
| `Write*` (`isSupervisor=false`) | `P=1`, `US=User`, **RW=1**, NX игнорируется | Успех → R=1, M=1 |
| `Write*` (`isSupervisor=true`)  | `P=1`, **RW=1** | Успех → R=1, M=1 |

Примечания:

- «User не может в Supervisor-страницы»: если `isSupervisor=false`, то доступ к странице с `US=Supervisor` — Page Fault.
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
