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

# Board family the fresh binary must exhibit. One engine spawn; turns a
# silent wrong-family link (stale shared src/*.o under a new EXE name) into
# a loud build error at the point of occurrence.
verify_build_board_family() {
    local board=normal
    if [[ "${BUILD_PROFILE}" == *"board=very-large"* ]]; then
        board=very-large
    elif [[ "${BUILD_PROFILE}" == *"board=large"* ]]; then
        board=large
    fi

    local catalog
    if ! catalog=$(printf 'uci\nsetoption name VariantPath value %s\nuci\nquit\n' \
        "${ROOT_DIR}/src/variants.ini" \
        | timeout "${FSX_BUILD_PROBE_TIMEOUT:-60s}" "${OUTPUT_FILE}" 2>&1); then
        echo "FAILED: ${EXE} board-family probe did not run (profile: ${BUILD_PROFILE})" >&2
        return 1
    fi

    local has_shogi=1 has_chu=1 has_hex16=1
    grep -q "var shogi\\([ ,]\\|\$\\)" <<<"${catalog}" || has_shogi=0
    grep -q "var chu_shogi\\([ ,]\\|\$\\)" <<<"${catalog}" || has_chu=0
    grep -q "var hex-16x16\\([ ,]\\|\$\\)" <<<"${catalog}" || has_hex16=0
    local fail=""
    case "${board}" in
        very-large)
            (( has_shogi )) || fail="expected shogi in catalog"
            (( has_chu )) || fail="expected chu_shogi in catalog"
            (( has_hex16 )) || fail="expected hex-16x16 in catalog"
            ;;
        large)
            (( has_shogi )) || fail="expected shogi in catalog"
            (( ! has_chu )) || fail="large binary must skip chu_shogi"
            (( ! has_hex16 )) || fail="non-VLB binary must skip hex-16x16"
            ;;
        *)
            (( ! has_shogi )) || fail="plain binary must skip shogi"
            (( ! has_chu )) || fail="plain binary must skip chu_shogi"
            (( ! has_hex16 )) || fail="plain binary must skip hex-16x16"
            ;;
    esac
    if [[ -n "${fail}" ]]; then
        echo "FAILED: ${EXE} board-family mismatch (profile: ${BUILD_PROFILE}): ${fail}" >&2
        return 1
    fi
}

validate_build_output() {
    if [[ ! -s "${OUTPUT_FILE}" || ! -x "${OUTPUT_FILE}" ]]; then
        echo "FAILED: ${EXE} build did not produce a runnable, non-empty executable" >&2
        exit 1
    fi
    if [[ "${COMPILER_KIND}" == mingw ]]; then
        case "$(uname -s)" in
            MINGW*|MSYS*|CYGWIN*) ;;
            *) echo "Skipping runtime board-family probe for MinGW cross-build."; return ;;
        esac
    fi
    verify_build_board_family
}

echo "Building ${EXE}..."

# A stale repo-root ./stockfish (ignored build artifact) is a known footgun:
# shells resolve ./stockfish before src/stockfish when run from the root.
# The wrapper never writes there, so drop it automatically when it is not
# the requested output.
if [[ "${OUTPUT_FILE}" != "${ROOT_DIR}/stockfish" && -e "${ROOT_DIR}/stockfish" ]]; then
    echo "Removing stale repo-root ./stockfish (use src/${EXE} instead)..."
    rm -f "${ROOT_DIR}/stockfish"
fi

if ! fsx_build_object_config_matches "$ROOT_DIR" "$@" \
    || ! fsx_build_signature_matches "$ROOT_DIR" "$OUTPUT_FILE" "$BUILD_SIGNATURE" "$BUILD_PROFILE"; then
    echo "Build configuration changed or artifact is unverified; cleaning objects..."
    rm -f "${OUTPUT_FILE}"
    # Drop the generated dependency file as well: it can reference headers
    # that no longer exist after refactors, which breaks the build outright.
    rm -f "${ROOT_DIR}/src/.depend"
    if ! make -C "${ROOT_DIR}/src" EXE="${EXE}" objclean >"${LOG_FILE}" 2>&1; then
        echo "FAILED: ${EXE} cleanup failed" >&2
        cat "${LOG_FILE}" >&2
        exit 1
    fi
fi

# A failed build can leave mixed objects; only a successful build certifies them.
rm -f "$(fsx_build_config_file "$ROOT_DIR")"

if [[ "${VERBOSE:-0}" == 1 ]]; then
    make -C "${ROOT_DIR}/src" -j $(nproc 2>/dev/null || echo 2) build "$@"
    validate_build_output
    fsx_build_write_signature "$ROOT_DIR" "$OUTPUT_FILE" "$BUILD_SIGNATURE" "$BUILD_PROFILE"
    fsx_build_write_object_config "$ROOT_DIR" "$@"
else
    if make -C "${ROOT_DIR}/src" -s -j $(nproc 2>/dev/null || echo 2) build "$@" >"${LOG_FILE}" 2>&1; then
        validate_build_output
        fsx_build_write_signature "$ROOT_DIR" "$OUTPUT_FILE" "$BUILD_SIGNATURE" "$BUILD_PROFILE"
        fsx_build_write_object_config "$ROOT_DIR" "$@"
        echo "ok: ${EXE} built successfully"
    else
        echo "FAILED: ${EXE} build failed" >&2
        echo "=================== Compile Log ===================" >&2
        cat "${LOG_FILE}" >&2
        exit 1
    fi
fi
