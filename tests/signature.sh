#!/bin/bash
# obtain and optionally verify Bench / signature
# if no reference is given, the output is deliberately limited to just the signature

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${2:-}" "${3:-}" "signature regression"

# Bench can take longer in debug CI builds than the default UCI timeout.
bench_output="$(UCI_TIMEOUT=15m run_engine_stdin "$ENGINE" $'bench\nquit\n')"
signature="$(printf '%s\n' "$bench_output" | bench_nodes)"

if [ -z "$signature" ]; then
   echo "No signature obtained from bench. Code crashed or assert triggered ?"
   printf '%s\n' "$bench_output"
   exit 1
fi

if [ $# -gt 0 ] && [ -n "$1" ]; then
   # compare to given reference
   if [ "$1" != "$signature" ]; then
      echo "signature mismatch: reference $1 obtained: $signature ."
      exit 1
   else
      echo "signature OK: $signature"
   fi
else
   # just report signature
   echo "$signature"
fi
