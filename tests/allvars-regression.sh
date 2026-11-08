#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
PYTHON=${PYTHON:-python3}
JOBS=${JOBS:-2}

mkdir -p "${ROOT_DIR}/.local/build"
tmp_dir="$(mktemp -d "${ROOT_DIR}/.local/build/allvars-regression.XXXXXX")"
trap 'rm -rf "${tmp_dir}"' EXIT

ENGINE="${1:-"${tmp_dir}/stockfish-allvars"}"

run_step() {
  local label="$1"
  shift
  echo "== ${label} =="
  local time_bin=${FSX_TIME_BIN:-/usr/bin/time}
  if [[ -x "${time_bin}" ]]; then "${time_bin}" -f "elapsed %es" "$@"; else "$@"; fi
}

cd "${ROOT_DIR}"

if [[ $# -eq 0 ]]; then
  run_step "build all-vars engine" timeout 30m tests/build.sh ARCH=x86-64-modern largeboards=yes all=yes EXE="${ENGINE}"
fi
run_step "fast regression" timeout 30m bash tests/run.sh fast "${ENGINE}"
run_step "variant perft" timeout 30m bash tests/perft.sh all "${ENGINE}"

echo "all-vars regression suite passed"
