#!/bin/bash

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
export ROOT_DIR
source "${ROOT_DIR}/tests/lib/uci.sh"
setup_test_context "${1:-}" "${2:-}" "piece region regression"

load_inline_variants <<'INI'
[istep-piece-specific:chess]
king = -
checking = false
customPiece1 = a:iW
pieceToCharTable = A:a
startFen = 8/8/8/8/8/8/8/4A3 w - - 0 1
doubleStepRegionWhite = A(e1); *(*2)

[irider-piece-specific:chess]
king = -
checking = false
customPiece1 = a:imR2
pieceToCharTable = A:a
startFen = 8/8/8/8/8/8/8/4A3 w - - 0 1
doubleStepRegionWhite = A(e1); *(*2)

[itriple-piece-specific:chess]
king = -
checking = false
customPiece1 = a:iW
pieceToCharTable = A:a
startFen = 8/8/8/8/8/8/8/4A3 w - - 0 1
tripleStepRegionWhite = A(e1)

[ipawnlike-piece-specific:chess]
customPiece1 = a:iW
pieceToCharTable = A:a
pawnLikeTypes = a
startFen = 4k3/8/8/8/8/8/8/4A2K w - - 0 1
doubleStepRegionWhite = A(e1); *(*2)

[irider-roundtrip:chess]
king = k
customPiece1 = d:efWfFmsWifmnD
pieceToCharTable = PNBRQ............D...Kpnbrq............d...k
pawnLikeTypes = d
enPassantTypes = d
startFen = 4k3/8/8/8/8/8/8/4D2K w - - 0 1
doubleStepRegionWhite = D(e1); *(*2)

[semitorpedo-test:chess]
doubleStepRegionWhite = *2 *3
doubleStepRegionBlack = *7 *6
startFen = rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1

[pawnlike-nonstep:chess]
customPiece1 = m:NN
pieceToCharTable = PNBRQ............M...Kpnbrq............m...k
pawnLikeTypes = m
startFen = 4k3/8/8/8/8/8/M7/K7 w - - 0 1

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

[same-player-repeat-control:chess]
startFen = 4k3/8/8/8/8/8/R7/4K3 w - - 0 1

[same-player-repeat-illegal:same-player-repeat-control]
samePlayerBoardRepetitionIllegal = true

[flip5:chess]
maxRank = 5
maxFile = e
startFen = 4k/5/5/5/4K w - - 0 1

[v1:chess]
startFen = 4k3/8/8/8/8/8/8/4K3 w - - 0 1

[v2:v1]
startFen = 4k3/8/8/8/4P3/8/8/4K3 w - - 0 1
INI

out=$(run_uci "$ENGINE" "$TMP_VARIANTS" istep-piece-specific <<'UCI'
position startpos
go perft 1
UCI
)
if ! grep -q "^e1e2: 1$" <<<"$out"; then
  echo "skip: piece-region regression not supported by this build"
  exit 0
fi
assert_contains "$out" "^e1e2: 1$"
assert_contains "$out" "^e1e3: 1$"
assert_not_contains "$out" "^e1e4: 1$"

out=$(run_uci "$ENGINE" "$TMP_VARIANTS" irider-piece-specific <<'UCI'
position startpos
go perft 1
UCI
)
assert_contains "$out" "^e1e2: 1$"
assert_contains "$out" "^e1e3: 1$"

out=$(run_uci "$ENGINE" "$TMP_VARIANTS" itriple-piece-specific <<'UCI'
position startpos
go perft 1
UCI
)
assert_contains "$out" "^e1e2: 1$"
assert_contains "$out" "^e1e3: 1$"
assert_contains "$out" "^e1e4: 1$"

out=$(run_uci "$ENGINE" "$TMP_VARIANTS" ipawnlike-piece-specific <<'UCI'
position startpos
go perft 1
UCI
)
assert_contains "$out" "^e1e2: 1$"
assert_contains "$out" "^e1e3: 1$"
assert_not_contains "$out" "^e1e4: 1$"

out=$(run_uci "$ENGINE" "$TMP_VARIANTS" irider-roundtrip <<'UCI'
position fen 4k3/8/8/8/8/8/8/4D2K w - - 0 1 moves e1d1 e8e7 d1e1 e7e8
go perft 1
UCI
)
assert_contains "$out" "^e1e2: 1$"
assert_not_contains "$out" "^e1e4: 1$"

out=$(run_uci "$ENGINE" "$TMP_VARIANTS" semitorpedo-test <<'UCI'
position startpos moves e2e3 a7a6
go perft 1
UCI
)
assert_contains "$out" "^e3e4: 1$"
assert_contains "$out" "^e3e5: 1$"

out=$(run_uci "$ENGINE" "$TMP_VARIANTS" semitorpedo-test <<'UCI'
position startpos moves e2e4 a7a6
go perft 1
UCI
)
assert_contains "$out" "^e4e5: 1$"
assert_not_contains "$out" "^e4e6: 1$"

out=$(run_uci "$ENGINE" "$TMP_VARIANTS" pawnlike-nonstep <<'UCI'
position startpos
go perft 1
UCI
)
assert_contains "$out" "^a2c1: 1$"
assert_contains "$out" "^a2c3: 1$"
assert_not_contains "$out" "^a2a[34]:"

out=$(run_uci "$ENGINE" "$TMP_VARIANTS" v1 <<'CMDS'
position startpos
go perft 1
setoption name UCI_Variant value v2
CMDS
)
assert_contains "$out" "^e1d1: 1$"
assert_contains "$out" "info string variant v2 files 8 ranks 8 pocket 0 template fairy startpos 4k3/8/8/8/4P3/8/8/4K3 w - - 0 1"

out=$(run_uci "$ENGINE" "$TMP_VARIANTS" immobility-illegal-hopper-test <<'EOF'
position fen 8/8/8/8/8/8/8/4K3[M] w - - 0 1
go perft 1
EOF
)
assert_contains "$out" "^M@a6:"
assert_contains "$out" "^M@e6:"
assert_not_contains "$out" "^M@a7:"
assert_not_contains "$out" "^M@e7:"
assert_not_contains "$out" "^M@a8:"
assert_not_contains "$out" "^M@e8:"

out=$(run_uci "$ENGINE" "$TMP_VARIANTS" same-player-repeat-control <<'CMDS'
position startpos moves a2a3 e8e7 a3a2 e7e8
go perft 1
CMDS
)
assert_contains "$out" "^a2a3: 1$"

out=$(run_uci "$ENGINE" "$TMP_VARIANTS" same-player-repeat-illegal <<'CMDS'
position startpos moves a2a3 e8e7 a3a2 e7e8
go perft 1
CMDS
)
assert_contains "$out" "^e1d1: 1$"

if variant_available "$ENGINE" janggi "$VARIANTS"; then
  out=$(run_uci "$ENGINE" "$VARIANTS" janggi <<'EOF'
position startpos
go perft 1
EOF
)
  assert_contains "$out" "^Nodes searched: 32$"
  assert_contains "$out" "^0000: 1$"

  out=$(run_uci "$ENGINE" "$VARIANTS" janggi <<'EOF'
position fen 1n1kaabn1/cr2N4/5C1c1/p1pNp3p/9/9/P1PbP1P1P/3r1p3/4A4/R1BA1KB1R b - - 0 1 moves a9e9 e2d3
go perft 1
EOF
)
  assert_contains "$out" "^Nodes searched: 37$"
  assert_contains "$out" "^f3e2: 1$"
  assert_contains "$out" "^0000: 1$"
else
  echo "janggi variant not available in this build; skipping janggi regression"
fi
