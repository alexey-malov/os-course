#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cmake -S "$ROOT" -B "$ROOT/build/release" -DCMAKE_BUILD_TYPE=Release
cmake --build "$ROOT/build/release" --parallel
ctest --test-dir "$ROOT/build/release" --output-on-failure
POOL_SIZE=1 "$ROOT/scripts/run-experiments.sh" demo
printf '\nStarter baseline is OK. Student scheduling functions are still TODO.\n'
