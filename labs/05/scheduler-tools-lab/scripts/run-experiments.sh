#!/usr/bin/env bash
# Примеры автоматизации экспериментов Linux без прав суперпользователя.
set -Eeuo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build/release}"
OUT_DIR="${OUT_DIR:-$ROOT/results}"
BIN_DIR="$BUILD_DIR/bin"
REPEATS="${REPEATS:-3}"
LOCAL_ITERATIONS="${LOCAL_ITERATIONS:-2500000000}"
SHARED_ITERATIONS="${SHARED_ITERATIONS:-100000000}"
PRIORITY_ITERATIONS="${PRIORITY_ITERATIONS:-1000000000}"
PRIORITY_HOG_MS="${PRIORITY_HOG_MS:-30000}"
IO_HOG_MS="${IO_HOG_MS:-8000}"
IO_PERIOD_MS="${IO_PERIOD_MS:-10}"
IO_MESSAGES="${IO_MESSAGES:-300}"
HOG_PID=""

usage() {
    cat <<'USAGE'
Usage: scripts/run-experiments.sh info|scaling|priority|latency|all

The script builds a Release configuration, then writes CSV files to results/.
Environmental overrides: REPEATS, LOCAL_ITERATIONS, SHARED_ITERATIONS,
PRIORITY_ITERATIONS, PRIORITY_HOG_MS, IO_HOG_MS, IO_MESSAGES,
IO_PERIOD_MS, CPU_A, CPU_B, BUILD_DIR, OUT_DIR.

Examples:
  scripts/run-experiments.sh info
  REPEATS=1 scripts/run-experiments.sh scaling
  REPEATS=3 scripts/run-experiments.sh all
USAGE
}

cleanup_hog() {
    if [[ -n "$HOG_PID" ]]; then
        kill "$HOG_PID" 2>/dev/null || true
        wait "$HOG_PID" 2>/dev/null || true
        HOG_PID=""
    fi
}
trap cleanup_hog EXIT

number_check() {
    local name="$1" value="$2"
    if [[ ! "$value" =~ ^[1-9][0-9]*$ ]]; then
        echo "Expected positive integer for $name, got: $value" >&2
        exit 2
    fi
}

read_cpu_ids() {
    local allowed section first last i
    allowed="$(awk '/^Cpus_allowed_list:/ {print $2}' /proc/self/status)"
    if [[ -z "$allowed" ]]; then
        echo "Could not read Cpus_allowed_list from /proc/self/status" >&2
        exit 2
    fi
    IFS=',' read -ra sections <<< "$allowed"
    CPU_IDS=()
    for section in "${sections[@]}"; do
        if [[ "$section" == *-* ]]; then
            first="${section%-*}"
            last="${section#*-}"
            for ((i=first; i<=last; i++)); do
                CPU_IDS+=("$i")
            done
        else
            CPU_IDS+=("$section")
        fi
    done
    CPU_A="${CPU_A:-${CPU_IDS[0]}}"
    CPU_B="${CPU_B:-${CPU_IDS[1]:-${CPU_IDS[0]}}}"
}

prepare() {
    for command in cmake taskset chrt nice awk; do
        if ! command -v "$command" >/dev/null 2>&1; then
            echo "Missing command: $command" >&2
            exit 2
        fi
    done
    number_check REPEATS "$REPEATS"
    number_check LOCAL_ITERATIONS "$LOCAL_ITERATIONS"
    number_check SHARED_ITERATIONS "$SHARED_ITERATIONS"
    number_check PRIORITY_ITERATIONS "$PRIORITY_ITERATIONS"
    number_check PRIORITY_HOG_MS "$PRIORITY_HOG_MS"
    number_check IO_HOG_MS "$IO_HOG_MS"
    number_check IO_MESSAGES "$IO_MESSAGES"
    number_check IO_PERIOD_MS "$IO_PERIOD_MS"
    read_cpu_ids
    mkdir -p "$OUT_DIR"
    cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release >/dev/null
    cmake --build "$BUILD_DIR" --parallel 2 >/dev/null
}

get_value() {
    local content="$1" key="$2"
    awk -F= -v key="$key" '$1 == key {print $2; exit}' <<< "$content"
}

cpu_set() {
    local count="$1" value="" i
    for ((i=0; i<count; i++)); do
        [[ -z "$value" ]] || value+=","
        value+="${CPU_IDS[i]}"
    done
    echo "$value"
}

print_info() {
    echo "Kernel: $(uname -r)"
    echo "Allowed CPUs: $(awk '/^Cpus_allowed_list:/ {print $2}' /proc/self/status)"
    echo "Chosen CPU_A=$CPU_A; CPU_B=$CPU_B"
    echo "CPU quota (cgroup v2, if present):"
    if [[ -r /sys/fs/cgroup/cpu.max ]]; then
        cat /sys/fs/cgroup/cpu.max
    else
        echo "not available"
    fi
    echo "Output: $OUT_DIR"
}

scaling() {
    local csv="$OUT_DIR/scaling.csv" cpucounts=(1) thread_counts=(1 2 4 8)
    local cpus threads run program iterations output elapsed cpu checksum
    if ((${#CPU_IDS[@]} >= 2)); then cpucounts+=(2); fi
    if ((${#CPU_IDS[@]} >= 4)); then cpucounts+=(4); fi
    echo 'program,threads,cpu_count,cpu_set,repeat,elapsed_ms,process_cpu_ms,checksum' > "$csv"
    for program in cpu-local cpu-shared; do
        if [[ "$program" == cpu-local ]]; then iterations="$LOCAL_ITERATIONS";
        else iterations="$SHARED_ITERATIONS"; fi
        for cpus in "${cpucounts[@]}"; do
            local cpulist
            cpulist="$(cpu_set "$cpus")"
            for threads in "${thread_counts[@]}"; do
                for ((run=1; run<=REPEATS; run++)); do
                    output="$(taskset -c "$cpulist" "$BIN_DIR/$program" --threads "$threads" --iterations "$iterations")"
                    elapsed="$(get_value "$output" elapsed_ms)"
                    cpu="$(get_value "$output" process_cpu_ms)"
                    checksum="$(get_value "$output" checksum)"
                    echo "$program,$threads,$cpus,\"$cpulist\",$run,$elapsed,$cpu,$checksum" >> "$csv"
                    echo "scaling: $program, threads=$threads, cpus=$cpulist, run=$run, elapsed=${elapsed}ms"
                done
            done
        done
    done
    echo "Saved: $csv"
}

start_hog() {
    local ms="$1" log="$2"
    taskset -c "$CPU_A" "$BIN_DIR/cpu-local" --threads 1 --duration-ms "$ms" > "$log" 2>&1 &
    HOG_PID=$!
    sleep 0.2
    if ! kill -0 "$HOG_PID" 2>/dev/null; then
        echo "Background CPU competitor failed to start" >&2
        wait "$HOG_PID" || true
        HOG_PID=""
        exit 2
    fi
}

priority() {
    local csv="$OUT_DIR/priority.csv" contender niceness run output elapsed cpu alive
    echo 'competitor,nice,repeat,elapsed_ms,process_cpu_ms,competitor_alive_at_end' > "$csv"
    for contender in no yes; do
        for niceness in 0 10; do
            for ((run=1; run<=REPEATS; run++)); do
                if [[ "$contender" == yes ]]; then
                    start_hog "$PRIORITY_HOG_MS" "$OUT_DIR/priority_hog.log"
                fi
                output="$(nice -n "$niceness" taskset -c "$CPU_A" \
                    "$BIN_DIR/cpu-local" --threads 1 --iterations "$PRIORITY_ITERATIONS")"
                elapsed="$(get_value "$output" elapsed_ms)"
                cpu="$(get_value "$output" process_cpu_ms)"
                alive="na"
                if [[ "$contender" == yes ]]; then
                    alive="no"
                    if kill -0 "$HOG_PID" 2>/dev/null; then alive="yes"; fi
                    cleanup_hog
                    if [[ "$alive" != yes ]]; then
                        echo "WARNING: competitor ended before measured run; increase PRIORITY_HOG_MS" >&2
                    fi
                fi
                echo "$contender,$niceness,$run,$elapsed,$cpu,$alive" >> "$csv"
                echo "priority: contender=$contender, nice=$niceness, run=$run, elapsed=${elapsed}ms"
            done
        done
    done
    echo "Saved: $csv"
}

run_io_case() {
    local label="$1" policy="$2" niceness="$3" competitor="$4" run="$5"
    local producer_log="$OUT_DIR/${label}_${run}_producer.log"
    local consumer_log="$OUT_DIR/${label}_${run}_consumer.log"
    local max_late output mean p95 p99 max count

    if [[ "$competitor" == yes ]]; then
        start_hog "$IO_HOG_MS" "$OUT_DIR/io_hog.log"
    fi

    taskset -c "$CPU_B" "$BIN_DIR/io-pipe" --producer \
        --period-ms "$IO_PERIOD_MS" --messages "$IO_MESSAGES" 2> "$producer_log" |
        nice -n "$niceness" chrt "$policy" 0 taskset -c "$CPU_A" \
            "$BIN_DIR/io-pipe" --consumer > "$consumer_log"

    output="$(cat "$consumer_log")"
    mean="$(get_value "$output" latency_mean_ms)"
    p95="$(get_value "$output" latency_p95_ms)"
    p99="$(get_value "$output" latency_p99_ms)"
    max="$(get_value "$output" latency_max_ms)"
    count="$(get_value "$output" messages_received)"
    max_late="$(get_value "$(cat "$producer_log")" wakeup_max_ms)"
    echo "$label,$niceness,$competitor,$run,$count,$mean,$p95,$p99,$max,$max_late" >> "$OUT_DIR/latency.csv"
    echo "latency: $label, run=$run, p99=${p99}ms, producer max wakeup lateness=${max_late}ms"
    cleanup_hog
}

latency() {
    local csv="$OUT_DIR/latency.csv" run
    if [[ "$CPU_A" == "$CPU_B" ]]; then
        echo "Need at least two allowed CPUs for controlled producer/consumer experiment." >&2
        return 1
    fi
    echo 'policy,nice,competitor,repeat,messages_received,mean_ms,p95_ms,p99_ms,max_ms,producer_wakeup_max_ms' > "$csv"
    for ((run=1; run<=REPEATS; run++)); do
        run_io_case 'other_baseline' '-o' 0 no "$run"
        run_io_case 'other_competing' '-o' 0 yes "$run"
        run_io_case 'other_nice10' '-o' 10 yes "$run"
        if chrt -b 0 true >/dev/null 2>&1; then
            run_io_case 'batch_competing' '-b' 0 yes "$run"
        else
            echo 'SCHED_BATCH launch unavailable; skipped' >&2
        fi
        if chrt -i 0 true >/dev/null 2>&1; then
            run_io_case 'idle_competing' '-i' 0 yes "$run"
        else
            echo 'SCHED_IDLE launch unavailable; skipped' >&2
        fi
    done
    echo "Saved: $csv; see *_producer.log to check wakeup lateness."
}

main() {
    local action="${1:-help}"
    if [[ "$action" == help || "$action" == --help || "$action" == -h ]]; then
        usage
        return 0
    fi
    case "$action" in
        info|scaling|priority|latency|all) ;;
        *) usage >&2; return 2 ;;
    esac
    prepare
    print_info
    case "$action" in
        info) ;;
        scaling) scaling ;;
        priority) priority ;;
        latency) latency ;;
        all) scaling; priority; latency ;;
    esac
}
main "$@"
