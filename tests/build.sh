#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
LOG_DIR="${ROOT_DIR}/.local/build"
LOG_FILE="${LOG_DIR}/compile.log"
source "${ROOT_DIR}/tests/lib/build-signature.sh"

mkdir -p "${LOG_DIR}"

# Extract EXE and compiler family from args. Make defaults to stockfish.exe for
# MinGW, so keep the wrapper's artifact tracking aligned when EXE is omitted.
EXE="stockfish"
EXE_EXPLICIT=0
COMPILER_KIND="${COMP:-}"
for arg in "$@"; do
    case "$arg" in
        EXE=*) EXE="${arg#EXE=}"; EXE_EXPLICIT=1 ;;
        COMP=*) COMPILER_KIND="${arg#COMP=}" ;;
    esac
done
if (( ! EXE_EXPLICIT )) && [[ "${COMPILER_KIND}" == mingw ]]; then
    EXE="stockfish.exe"
fi

OUTPUT_FILE=$(fsx_build_output_path "$ROOT_DIR" "$EXE")
BUILD_SIGNATURE=$(fsx_build_signature "$ROOT_DIR" "$OUTPUT_FILE" "$@")
BUILD_PROFILE=$(fsx_build_profile "$@")
OBJECT_PROFILE_FILE="${LOG_DIR}/objects.profile"

object_profile_matches() {
    [[ -f "${OBJECT_PROFILE_FILE}" ]] \
        && [[ "$(sed -n '1p' "${OBJECT_PROFILE_FILE}")" == "${BUILD_PROFILE}" ]]
}

record_object_profile() {
    local temp_file="${OBJECT_PROFILE_FILE}.tmp.$$"
    printf '%s\n' "${BUILD_PROFILE}" >"${temp_file}"
    mv -f "${temp_file}" "${OBJECT_PROFILE_FILE}"
}

validate_build_output() {
    if [[ ! -s "${OUTPUT_FILE}" || ! -x "${OUTPUT_FILE}" ]]; then
        echo "FAILED: ${EXE} build did not produce a runnable, non-empty executable" >&2
        exit 1
    fi
}

echo "Building ${EXE}..."

if ! object_profile_matches; then
    echo "Object build profile changed; cleaning objects..."
    make -C "${ROOT_DIR}/src" EXE="${EXE}" objclean
fi

if ! fsx_build_signature_matches "$ROOT_DIR" "$OUTPUT_FILE" "$BUILD_SIGNATURE" "$BUILD_PROFILE"; then
    echo "Build configuration changed or artifact is unverified; cleaning objects..."
    rm -f "${OUTPUT_FILE}"
    make -C "${ROOT_DIR}/src" EXE="${EXE}" objclean
fi

if [[ "${VERBOSE:-0}" == 1 ]]; then
    make -C "${ROOT_DIR}/src" -j $(nproc 2>/dev/null || echo 2) build "$@"
    validate_build_output
    fsx_build_write_signature "$ROOT_DIR" "$OUTPUT_FILE" "$BUILD_SIGNATURE" "$BUILD_PROFILE"
    record_object_profile
else
    if make -C "${ROOT_DIR}/src" -j $(nproc 2>/dev/null || echo 2) build "$@" >"${LOG_FILE}" 2>&1; then
        validate_build_output
        fsx_build_write_signature "$ROOT_DIR" "$OUTPUT_FILE" "$BUILD_SIGNATURE" "$BUILD_PROFILE"
        record_object_profile
        echo "ok: ${EXE} built successfully"
    else
        echo "FAILED: ${EXE} build failed" >&2
        echo "=================== Compile Log ===================" >&2
        cat "${LOG_FILE}" >&2
        exit 1
    fi
fi
