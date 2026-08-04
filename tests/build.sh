#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
LOG_DIR="${ROOT_DIR}/.local/build"
LOG_FILE="${LOG_DIR}/compile.log"
source "${ROOT_DIR}/tests/lib/build-signature.sh"

mkdir -p "${LOG_DIR}"

# Extract EXE from args or default to stockfish
EXE="stockfish"
for arg in "$@"; do
    if [[ "$arg" =~ ^EXE=(.*) ]]; then
        EXE="${BASH_REMATCH[1]}"
    fi
done

OUTPUT_FILE=$(fsx_build_output_path "$ROOT_DIR" "$EXE")
BUILD_SIGNATURE=$(fsx_build_signature "$ROOT_DIR" "$OUTPUT_FILE" "$@")

echo "Building ${EXE}..."

if ! fsx_build_signature_matches "$ROOT_DIR" "$OUTPUT_FILE" "$BUILD_SIGNATURE"; then
    echo "Build configuration changed or artifact is unverified; cleaning objects..."
    make -C "${ROOT_DIR}/src" EXE="${EXE}" objclean
fi

if [[ "${VERBOSE:-0}" == 1 ]]; then
    make -C "${ROOT_DIR}/src" -j $(nproc 2>/dev/null || echo 2) build "$@"
    fsx_build_write_signature "$ROOT_DIR" "$OUTPUT_FILE" "$BUILD_SIGNATURE"
else
    if make -C "${ROOT_DIR}/src" -j $(nproc 2>/dev/null || echo 2) build "$@" >"${LOG_FILE}" 2>&1; then
        fsx_build_write_signature "$ROOT_DIR" "$OUTPUT_FILE" "$BUILD_SIGNATURE"
        echo "ok: ${EXE} built successfully"
    else
        echo "FAILED: ${EXE} build failed" >&2
        echo "=================== Compile Log ===================" >&2
        cat "${LOG_FILE}" >&2
        exit 1
    fi
fi
