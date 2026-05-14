#!/usr/bin/env bash
set -euo pipefail

ENGINE="${1:-src/stockfish}"

run_engine() {
    BENCH_SCRIPT="$1" python - "$ENGINE" 2>&1 <<'PY'
import subprocess
import sys
import os

engine = sys.argv[1]
script = os.environ["BENCH_SCRIPT"]
res = subprocess.run([engine], input=script, text=True, capture_output=True)
sys.stdout.write(res.stdout)
sys.stdout.write(res.stderr)
PY
}

baseline_output="$(run_engine $'uci\nbench 16 1 1 default depth\nquit\n')"
baseline_nodes="$(printf '%s\n' "$baseline_output" | grep -F "Nodes searched" | tail -1 | awk '{print $4}')"
if [[ -z "${baseline_nodes}" ]]; then
    printf '%s\n' "$baseline_output"
    echo "bench stdin regression failed to produce baseline node count"
    exit 1
fi

output="$(run_engine $'uci\nsetoption name Threads value 4\nsetoption name Hash value 32\nbench 0 0 1 default depth\nquit\n')"
nodes="$(printf '%s\n' "$output" | grep -F "Nodes searched" | tail -1 | awk '{print $4}')"
if [[ -z "${nodes}" ]]; then
    printf '%s\n' "$output"
    echo "bench stdin regression failed to produce node count"
    exit 1
fi

if [[ "${nodes}" != "${baseline_nodes}" ]]; then
    printf '%s\n' "$output"
    echo "bench stdin regression did not reset zero hash/thread arguments to defaults"
    exit 1
fi

echo "bench stdin regression passed"
