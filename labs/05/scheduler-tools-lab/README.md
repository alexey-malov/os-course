### scheduler-tools: заготовки для лабораторной работы

**Полный текст задания:** [README.md](../README.md).

Заготовки предназначены для Linux, C++20, CMake >= 3.20 и GCC/Clang со стандартной библиотекой,
поддерживающей `std::jthread` и `std::latch`.

#### Структура

```text
scheduler-tools-lab/
├── README.md
├── CMakeLists.txt
├── cpu-local/
│   ├── CMakeLists.txt
│   └── src/main.cpp
├── cpu-shared/
│   ├── CMakeLists.txt
│   └── src/main.cpp
├── io-pipe/
│   ├── CMakeLists.txt
│   └── src/main.cpp
└── scripts/
    └── run-experiments.sh
```

Все три программы являются самостоятельными проектами CMake и одновременно собираются из корня.

#### Сборка Release

```bash
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release --parallel
```

Самостоятельная сборка одного проекта, например:

```bash
cmake -S cpu-local -B build/local -DCMAKE_BUILD_TYPE=Release
cmake --build build/local --parallel
```

Исполняемые файлы будут расположены в `build/release/bin/` или `build/local/bin/` соответственно.

#### Примеры ручного запуска

Выяснить разрешённые CPU:

```bash
grep Cpus_allowed_list /proc/self/status
```

Для следующих команд замените `CPU_A` и `CPU_B` допустимыми номерами:

```bash
CPU_A=0
CPU_B=1

taskset -c "$CPU_A" build/release/bin/cpu-local \
  --threads 4 --iterations 2500000000

taskset -c "$CPU_A" build/release/bin/cpu-shared \
  --threads 4 --iterations 100000000

taskset -c "$CPU_B" build/release/bin/io-pipe \
  --producer --period-ms 10 --messages 300 |
  taskset -c "$CPU_A" build/release/bin/io-pipe --consumer
```

`--iterations` — **общее число итераций**, разделяемое между всеми потоками.
У `cpu-local` также есть режим длительной нагрузки `--duration-ms` для создания CPU-конкурента.
Основная нагрузка `cpu-local` использует локальную `volatile`-переменную;
это не средство синхронизации.
В `cpu-shared` есть общий атомарный счётчик с `memory_order_relaxed`.

`io-pipe --producer` отправляет бинарные сообщения в stdout, а статистику пробуждений пишет в **stderr**.
`io-pipe --consumer` читает сообщения из stdin и выводит среднюю задержку, P50, P95, P99 и максимум (миллисекунды).
Оба процесса используют один `CLOCK_MONOTONIC`. Результат включает задержки планирования **и** расходы передачи через pipe.

#### Скрипт экспериментов

```bash
scripts/run-experiments.sh info
scripts/run-experiments.sh scaling
scripts/run-experiments.sh priority
scripts/run-experiments.sh latency
scripts/run-experiments.sh all
```

Скрипт конфигурирует и собирает **Release** перед каждым запуском и сохраняет в `results/` файлы:

- `scaling.csv` — сравнение масштабирования `cpu-local` / `cpu-shared`;
- `priority.csv` — nice 0/10, с конкурентом и без него;
- `latency.csv` — задержки Consumer для разных политик;
- `*_producer.log` — статистику задержек пробуждения Producer.

По умолчанию: `REPEATS=3`, `LOCAL_ITERATIONS=2500000000`,
`SHARED_ITERATIONS=100000000`, `PRIORITY_ITERATIONS=1000000000`, `IO_MESSAGES=300`,
`IO_PERIOD_MS=10`. Для быстрой проверки уменьшите значения:

```bash
REPEATS=1 LOCAL_ITERATIONS=1000000 SHARED_ITERATIONS=100000 \
  scripts/run-experiments.sh scaling
```

`CPU_A` и `CPU_B` по умолчанию берутся из `/proc/self/status`.
Если в среде есть квота cgroup по CPU, максимальное ускорение может быть ограничено **даже при большой маске affinity**.

**Эксперимент с renice выполняется вручную.**

Запустите программу cpu-local с несколькими потоками.
Во время её выполнения определите TID одного из рабочих потоков и измените его значение nice с помощью renice.

Убедитесь, что изменение применилось.
Исследуйте, как изменение nice отдельного потока влияет на распределение
процессорного времени между потоками при конкуренции за один CPU.

Объясните полученные результаты.

#### Важные предостережения

- Скрипт не устанавливает политики `SCHED_FIFO` или `SCHED_RR` и не требует `sudo`.
- `SCHED_IDLE` при наличии постоянного CPU-конкурента может привести к существенным задержкам Consumer;
  число сообщений и продолжительность конкурента конечны.
- Если фоновой процесс завершился раньше измеряемого запуска,
  сравнение с конкурентом нужно повторить с большей длительностью фона.
- Использование нескольких потоков на разных CPU не гарантирует линейное ускорение.
  В частности, общий атомарный счётчик может ухудшать масштабирование.
- Скрипт не строит графики и не подменяет анализ полученных данных.
