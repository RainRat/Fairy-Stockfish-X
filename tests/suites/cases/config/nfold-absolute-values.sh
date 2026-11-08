#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/../../../../tests/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "Absolute n-fold result regression test"

CXX=${CXX:-g++}
JOBS=${JOBS:-2}
source "${SCRIPT_DIR}/../../../../tests/lib/harness-build.sh"
fsx_harness_init "${ENGINE}" "${ROOT_DIR}"
fsx_harness_prepare_objects "${JOBS}"
fsx_harness_collect_objects

BUILD_DIR="${ROOT_DIR}/.local/build/nfold-absolute-values"
mkdir -p "${BUILD_DIR}"
fsx_harness_build "${SCRIPT_DIR}/nfold-absolute-values.cpp" \
  "${BUILD_DIR}/nfold-absolute-values.bin" "nfold-absolute-values"

"${BUILD_DIR}/nfold-absolute-values.bin"
echo "absolute n-fold result regression passed"
