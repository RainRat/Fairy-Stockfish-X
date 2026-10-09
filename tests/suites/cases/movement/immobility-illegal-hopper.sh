#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/../../../../tests/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "Immobility-illegal hopper"

load_inline_variants <<'INI'
[immobility-illegal-hopper-test:chess]
maxFile = h
maxRank = 8
pieceDrops = true
immobilityIllegal = true
king = k:W
customPiece1 = m:fpR
customPiece2 = g:W
promotedPieceType = m:g
startFen = 8/8/8/8/8/8/8/4K3[M]
INI

out=$(run_uci "${ENGINE}" "${FSX_TMP_INI}" immobility-illegal-hopper-test <<'UCI'
position fen 8/8/8/8/8/8/8/4K3[M] w - - 0 1
go perft 1
UCI
)
assert_contains "$out" "^M@a6: 1$"
assert_contains "$out" "^M@e6: 1$"
assert_not_contains "$out" "^M@a7:"
assert_not_contains "$out" "^M@e7:"
assert_not_contains "$out" "^M@a8:"
assert_not_contains "$out" "^M@e8:"
