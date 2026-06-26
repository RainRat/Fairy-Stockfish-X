#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
PYTHON=${PYTHON:-python3}
JOBS=${JOBS:-2}

tmp_dir="$(mktemp -d)"
trap 'rm -rf "${tmp_dir}"' EXIT

ENGINE="${1:-"${tmp_dir}/stockfish-allvars"}"

run_step() {
  local label="$1"
  shift
  echo "== ${label} =="
  /usr/bin/time -f "elapsed %es" "$@"
}

cd "${ROOT_DIR}"

if [[ $# -eq 0 ]]; then
  run_step "clean all-vars objects" timeout 2m make -C src EXE="${ENGINE}" objclean
  run_step "build all-vars engine" timeout 30m make -C src -j"${JOBS}" build ARCH=x86-64 largeboards=yes all=yes nnue=yes EXE="${ENGINE}"
fi
run_step "fast regression" timeout 30m bash tests/fast-regression.sh "${ENGINE}"
run_step "variant perft" timeout 30m bash tests/perft.sh all "${ENGINE}"

echo "all-vars regression suite passed"
