#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/../../../../tests/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "standard lazy evaluation parity"

load_inline_variants <<'INI'
[two-step-chess:chess]
twoStepMoves = q:N>N

[hook-chess:chess]
hookMoves = q:R-R
INI

for variant in two-step-chess hook-chess; do
  trace=$(run_uci "${ENGINE}" "${FSX_TMP_INI}" "${variant}" <<'UCI'
setoption name Use NNUE value false
position startpos
eval
UCI
)
  assert_contains "${trace}" "^Final evaluation"
done

echo "standard lazy evaluation parity regression passed"
