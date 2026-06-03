#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "bench stdin regression"

baseline_output="$(run_engine_stdin "$ENGINE" $'uci\nbench 16 1 1 default depth\nquit\n')"
baseline_nodes="$(printf '%s\n' "$baseline_output" | bench_nodes)"
if [[ -z "${baseline_nodes}" ]]; then
    printf '%s\n' "$baseline_output"
    echo "bench stdin regression failed to produce baseline node count"
    exit 1
fi

output="$(run_engine_stdin "$ENGINE" $'uci\nsetoption name Threads value 4\nsetoption name Hash value 32\nbench 0 0 1 default depth\nquit\n')"
nodes="$(printf '%s\n' "$output" | bench_nodes)"
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
