#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP="${APP:-$ROOT/build/release/game-sim}"
RESULTS="${RESULTS:-$ROOT/results}"
DURATION="${DURATION:-8}"
ASSET_THREADS="${ASSET_THREADS:-4}"
POOL_SIZE="${POOL_SIZE:-2}"
REPEATS="${REPEATS:-1}"
MODE="${1:-demo}"
SCENE="${2:-A}"

mkdir -p "$RESULTS"
if [[ ! -x "$APP" ]]; then
  echo "No executable at $APP; build Release first (see README.md)." >&2
  exit 1
fi

# Prefer a fixed rate in every run; do not recalibrate when switching settings.
if [[ -n "${ITERS_PER_MS:-}" ]]; then
  ITERATIONS="$ITERS_PER_MS"
elif [[ -s "$RESULTS/iterations-per-ms.txt" ]]; then
  ITERATIONS="$(cat "$RESULTS/iterations-per-ms.txt")"
else
  ITERATIONS="$($APP --calibrate)"
  printf '%s\n' "$ITERATIONS" > "$RESULTS/iterations-per-ms.txt"
fi
if [[ ! "$ITERATIONS" =~ ^[0-9]+$ ]] || (( ITERATIONS <= 0 )); then
  echo "Invalid iteration rate: $ITERATIONS" >&2
  exit 1
fi

case "$POOL_SIZE" in
  1|2) ;;
  *) echo "POOL_SIZE must be 1 or 2 in this helper script" >&2; exit 1;;
esac

scenario_values() {
  case "$1" in
    A) FRAME_WORK=3; AI_WORK=3 ;;
    B) FRAME_WORK=9; AI_WORK=3 ;;
    C) FRAME_WORK=3; AI_WORK=12 ;;
    D) FRAME_WORK=9; AI_WORK=12 ;;
    *) echo "Unknown scene: $1 (use A,B,C,D)" >&2; exit 1 ;;
  esac
}

run_case() {
  local scene="$1" profile="$2" rep="$3" reference="${4:-}"
  scenario_values "$scene"
  local prefix="$RESULTS/${scene}-${profile}-${rep}"
  local args=(
    --duration "$DURATION"
    --pool-size "$POOL_SIZE"
    --audio-work-ms 0.8
    --frame-work-ms "$FRAME_WORK"
    --ai-work-ms "$AI_WORK"
    --asset-work-ms 2
    --asset-threads "$ASSET_THREADS"
    --iters-per-ms "$ITERATIONS"
    --summary-csv "$prefix.csv"
    --events-csv "$prefix-events.csv"
  )
  if [[ -n "$reference" ]]; then args+=(--asset-baseline-rate "$reference"); fi
  case "$profile" in
    baseline) ;;
    weighted) args+=(--ai-nice 5 --asset-nice 10) ;;
    split)
      if (( POOL_SIZE < 2 )); then echo "split requires POOL_SIZE=2" >&2; exit 1; fi
      args+=(--placement split --ai-nice 5 --asset-nice 10)
      ;;
    *) echo "Unknown profile: $profile" >&2; exit 1 ;;
  esac
  echo "[$scene] profile=$profile repeat=$rep -> $prefix.csv" >&2
  "$APP" "${args[@]}" > "$prefix.log" 2>&1 || {
    cat "$prefix.log" >&2
    echo "NOTE: for weighted/split profiles implement the TODOs in ThreadScheduling.cpp first." >&2
    return 1
  }
  grep -E '^(ASSET_THROUGHPUT|GAME_QUALITY_SCORE|ASSET_RELATIVE_PERCENT|ACCEPTABILITY)=' "$prefix.log" >&2
}

case "$MODE" in
  demo)
    DURATION=2
    POOL_SIZE=1
    run_case A baseline 1
    ;;
  baseline)
    run_case "$SCENE" baseline 1
    ;;
  matrix)
    echo "Running four scenes x (baseline, weighted, split) x $REPEATS repetitions" >&2
    if (( POOL_SIZE != 2 )); then
      echo "matrix uses split: set POOL_SIZE=2 and ensure two logical CPUs are available" >&2
      exit 1
    fi
    for scene in A B C D; do
      # A separate baseline for every scene and repeat.
      for ((rep=1; rep<=REPEATS; rep++)); do
        run_case "$scene" baseline "$rep"
        reference="$(awk -F= '/^ASSET_THROUGHPUT=/{print $2}' "$RESULTS/${scene}-baseline-${rep}.log")"
        if [[ -z "$reference" ]]; then echo "Cannot parse baseline rate" >&2; exit 1; fi
        run_case "$scene" weighted "$rep" "$reference"
        run_case "$scene" split "$rep" "$reference"
      done
    done
    echo "Done. Each run has summary CSV, events CSV and text log under $RESULTS." >&2
    ;;
  *)
    echo "Usage: scripts/run-experiments.sh demo | baseline [A|B|C|D] | matrix" >&2
    echo "Environment: DURATION=8 POOL_SIZE=2 REPEATS=1 ASSET_THREADS=4 ITERS_PER_MS=..." >&2
    exit 2
    ;;
esac
