#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
LOG_DIR="${ROOT_DIR}/.local/build"
LOG_FILE="${LOG_DIR}/compile.log"

mkdir -p "${LOG_DIR}"

# Extract EXE from args or default to stockfish
EXE="stockfish"
for arg in "$@"; do
    if [[ "$arg" =~ ^EXE=(.*) ]]; then
        EXE="${BASH_REMATCH[1]}"
    fi
done

echo "Building ${EXE}..."

if [[ "${VERBOSE:-0}" == 1 ]]; then
    make -C "${ROOT_DIR}/src" -j $(nproc 2>/dev/null || echo 2) build "$@"
else
    if make -C "${ROOT_DIR}/src" -j $(nproc 2>/dev/null || echo 2) build "$@" >"${LOG_FILE}" 2>&1; then
        echo "ok: ${EXE} built successfully"
    else
        echo "FAILED: ${EXE} build failed" >&2
        echo "=================== Compile Log ===================" >&2
        cat "${LOG_FILE}" >&2
        exit 1
    fi
fi
