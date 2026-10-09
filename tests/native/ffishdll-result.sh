#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/../.." && pwd)
ENGINE=${ENGINE:-${1:-${ROOT_DIR}/src/stockfish}}
if [[ $# -gt 0 ]]; then shift; fi

source "${ROOT_DIR}/tests/lib/harness-build.sh"
fsx_harness_init "${ENGINE}" "${ROOT_DIR}"
fsx_harness_prepare_objects_cached "${ROOT_DIR}/.local/build/ffishdll-result-objects" \
    "DLL result objects" "${JOBS:-2}"
fsx_harness_collect_objects

BUILD_DIR="${ROOT_DIR}/.local/build/ffishdll-result"
mkdir -p "${BUILD_DIR}"
exec 9>"${BUILD_DIR}/ffishdll-result.lock"
flock 9
fsx_harness_build "${ROOT_DIR}/tests/native/ffishdll-result.cpp" \
    "${BUILD_DIR}/ffishdll-result" "DLL result API" \
    "${BUILD_DIR}/ffishdll-result.sig" "${ROOT_DIR}/src/ffishdll.cpp"
"${BUILD_DIR}/ffishdll-result"
