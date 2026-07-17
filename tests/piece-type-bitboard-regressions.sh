#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
CXX=${CXX:-g++}
BUILD_DIR="${ROOT_DIR}/.local/build/piece-type-bitboard-regressions"
mkdir -p "${BUILD_DIR}"

"${CXX}" -std=c++17 -O2 -Wall -Wextra -I"${ROOT_DIR}/src" \
  "${ROOT_DIR}/tests/piece-type-bitboard-regressions.cpp" \
  -o "${BUILD_DIR}/piece-type-bitboard-regressions"
"${BUILD_DIR}/piece-type-bitboard-regressions"
