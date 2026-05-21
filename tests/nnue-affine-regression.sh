#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

CXX=${CXX:-g++}
COMMON=(-std=c++17 -O2 -Wall -Wextra -I"$ROOT/src")
case "$(uname -m)" in
  x86_64|amd64|i386|i686) ;;
  *)
    echo "nnue-affine-regression skipped: x86 SIMD flags are unavailable on $(uname -m)"
    exit 0
    ;;
esac

$CXX "${COMMON[@]}" \
  -DUSE_SSE2 -DUSE_SSSE3 \
  -msse2 -mssse3 \
  "$ROOT/tests/nnue-affine-regression.cpp" -o "$TMPDIR/nnue-affine-ssse3"
"$TMPDIR/nnue-affine-ssse3"

$CXX "${COMMON[@]}" \
  -DUSE_SSE2 -DUSE_SSSE3 -DUSE_SSE41 -DUSE_AVX2 \
  -msse2 -mssse3 -msse4.1 -mavx2 \
  "$ROOT/tests/nnue-affine-regression.cpp" -o "$TMPDIR/nnue-affine-avx2"
"$TMPDIR/nnue-affine-avx2"
