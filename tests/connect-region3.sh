#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "connect-region3 regression"

load_inline_variants <<'INI'
[mini-y:fairy]
maxRank = 5
maxFile = 5
hexBoard = true
pieceToCharTable = -
king = -
customPiece1 = s:m
pieceDrops = true
mustDrop = true
openingSwapDrop = true
connectPieceTypes = s
connectHorizontal = true
connectVertical = true
connectDiagonal = true
connectNorthEast = false
connectSouthEast = true
connectRegion1White = a1 b1 c1 d1 e1
connectRegion2White = a1 b2 c3 d4 e5
connectRegion3White = e1 e2 e3 e4 e5
connectRegion1Black = a1 b1 c1 d1 e1
connectRegion2Black = a1 b2 c3 d4 e5
connectRegion3Black = e1 e2 e3 e4 e5
nMoveRule = 0
startFen = ****1/***2/**3/*4/5[SSSSSSSSSSSSSSSsssssssssssssss] b - - 0 1
INI

tmp_ini="${FSX_TMP_INI}"

out=$(run_uci "$ENGINE" "$tmp_ini" mini-y <<'UCI'
position startpos
go perft 1
UCI
)
assert_nodes "$out" 15

out=$(run_uci "$ENGINE" "$tmp_ini" mini-y <<'UCI'
position fen ^^^^b/^^^1b/^^2b/^3b/bbbbb w - - 0 1
go perft 1
UCI
)
assert_nodes "$out" 0

out=$(run_uci "$ENGINE" "$tmp_ini" mini-y <<'UCI'
position fen ^^^^1/^^^2/^^3/^b1b1/b1b1b[S] w - - 0 1
go perft 1
UCI
)
assert_contains "$out" "^S@b1: 1$"

echo "connect-region3 regression passed"
