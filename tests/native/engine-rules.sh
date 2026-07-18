#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/../.." && pwd)
ENGINE=${ENGINE:-${1:-${ROOT_DIR}/src/stockfish}}
if [[ $# -gt 0 ]]; then shift; fi
VARIANTS=${VARIANTS:-${1:-${ROOT_DIR}/src/variants.ini}}
if [[ $# -gt 0 ]]; then shift; fi

source "${ROOT_DIR}/tests/lib/harness-build.sh"
fsx_harness_init "${ENGINE}" "${ROOT_DIR}"
fsx_harness_prepare_objects_cached "${ROOT_DIR}/.local/build/engine-rules-objects" "engine rules objects" "${JOBS:-2}"
fsx_harness_collect_objects
BUILD_DIR="${ROOT_DIR}/.local/build/engine-rules"
mkdir -p "${BUILD_DIR}"
# Fast suites may invoke different native groups concurrently.  Keep the
# shared executable stable while one group is rebuilding or running it.
exec 9>"${BUILD_DIR}/engine-rules.lock"
flock 9
fsx_harness_build "${ROOT_DIR}/tests/native/engine-rules.cpp" "${BUILD_DIR}/engine-rules" "engine rules"
"${BUILD_DIR}/engine-rules" "${VARIANTS}" "$@"
