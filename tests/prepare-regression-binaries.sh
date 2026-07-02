#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
JOBS=${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)}
LOG_DIR="${ROOT_DIR}/.local/build/regression-binaries"
mkdir -p "${LOG_DIR}"

build() {
  local name="$1"
  shift
  local log="${LOG_DIR}/${name}.log"

  echo "building ${name}"
  if ! make -s -C "${ROOT_DIR}/src" EXE="${name}" objclean >"${log}" 2>&1 \
      || ! make -s -C "${ROOT_DIR}/src" -j"${JOBS}" build \
          ARCH=x86-64-modern all=yes nnue=yes EXE="${name}" "$@" >>"${log}" 2>&1; then
    echo "failed building ${name}; log=${log#"${ROOT_DIR}/"}" >&2
    tail -80 "${log}" >&2
    return 1
  fi
}

# Object files are shared between build families. Build VLB first, then leave
# the tree in the normal large-board family used by focused regression harnesses.
build stockfish-vlb largeboards=yes verylargeboards=yes
build stockfish-allvars largeboards=yes
cp -f "${ROOT_DIR}/src/stockfish-allvars" "${ROOT_DIR}/src/stockfish-large"
chmod +x "${ROOT_DIR}/src/stockfish-large"

echo "regression binaries ready: src/stockfish-large src/stockfish-allvars src/stockfish-vlb"
