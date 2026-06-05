#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

mode="${1:-}"
if [[ "${mode}" == "--stdin" ]]; then
  setup_test_context "${2:-}" "${3:-}" "bench stdin regression"
else
  setup_test_context "${2:-}" "${3:-}" "signature regression"
fi

if [[ "${mode}" == "--stdin" ]]; then
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

  echo "bench stdin regression passed"
  exit 0
fi

# Bench can take longer in debug CI builds than the default UCI timeout.
bench_output="$(UCI_TIMEOUT=15m run_engine_stdin "$ENGINE" $'bench\nquit\n')"
signature="$(printf '%s\n' "$bench_output" | bench_nodes)"

if [[ -z "${signature}" ]]; then
  echo "No signature obtained from bench. Code crashed or assert triggered ?"
  printf '%s\n' "$bench_output"
  exit 1
fi

if [[ -n "${mode}" ]]; then
  # Compare to the requested reference.
  if [[ "${mode}" != "${signature}" ]]; then
    echo "signature mismatch: reference ${mode} obtained: ${signature}."
    exit 1
  fi
else
  echo "${signature}"
fi
