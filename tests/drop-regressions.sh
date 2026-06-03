#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "drop regressions"

load_inline_variants <<'INI'
[borrow-slide:fairy]
maxRank = 5
maxFile = e
pieceToCharTable = -
king = -
customPiece1 = a:-
pieceDrops = true
mustDrop = true
captureType = hand
captureToHandSide = owner
borrowOpponentDropsWhenEmpty = true
edgeInsertOnly = true
dropRegion = a* *5
edgeInsertTypes = a
edgeInsertRegion = a* *5
edgeInsertFrom = top left
pushingStrength = a:5
startFen = 5/5/5/5/5[a] w - - 0 1

[capture-to-hand-types-demo:fairy]
maxFile = h
maxRank = 7
king = k
rook = r
knight = n
lance = l
gold = g
promotedPieceType = l:g
pieceDrops = true
captureType = hand
captureToHandTypes = rl
promotionPieceTypes = -
doubleStep = false
castling = false
checking = true

[owner-hand-capture:fairy]
maxRank = 3
maxFile = c
pieceToCharTable = -
king = -
customPiece1 = a:W
pieceDrops = true
startFen = 3/1a1/1A1[A] w - - 0 1
checking = false
captureType = hand
captureToHandSide = owner

[owner-hand-eject:fairy]
maxRank = 5
maxFile = e
pieceToCharTable = -
king = -
customPiece1 = a:-
startFen = AAAAa/5/5/5/5[A] w - - 0 1
pieceDrops = true
mustDrop = true
checking = false
nMoveRule = 0
captureType = hand
captureToHandSide = owner
edgeInsertOnly = true
dropRegionWhite = a1 a2 a3 a4 a5 b5 c5 d5 e5
edgeInsertTypes = a
edgeInsertRegionWhite = a1 a2 a3 a4 a5 b5 c5 d5 e5
edgeInsertFromWhite = top left
pushingStrength = a:5
pushFirstColor = either
pushingRemoves = shove

[pairdrop:fairy]
pieceDrops = true
symmetricDropTypes = r

[shogi-pawn-drop-split-white:minishogi]
shogiPawnDropMateIllegalWhite = true
shogiPawnDropMateIllegalBlack = false
startFen = 2k2/5/2K2/5/1R1R1[P] w - - 0 1

[shogi-pawn-drop-split-black:minishogi]
shogiPawnDropMateIllegalWhite = true
shogiPawnDropMateIllegalBlack = false
startFen = 1r1r1/5/2k2/5/2K2[p] b - - 0 1

[capture-drop-control:chess]
captureType = hand
pieceDrops = true
pocketSize = 6
startFen = 4k3/8/8/4p3/8/8/8/4K3[Q] w - - 0 1

[capture-drop:capture-drop-control]
captureDrops = q

[capture-drop-self:capture-drop]
selfCapture = true
startFen = 4k3/8/8/8/4P3/8/8/4K3[Q] w - - 0 1

[dropcheck-split-white:chess]
pieceDrops = true
dropChecksWhite = false
dropChecksBlack = true
startFen = 4k3/8/8/8/8/8/8/4K3[R] w - - 0 1

[dropcheck-split-black:chess]
pieceDrops = true
dropChecksWhite = false
dropChecksBlack = true
startFen = 4k3/8/8/8/8/8/8/4K3[r] b - - 0 1

[dropmate-split-white:chess]
pieceDrops = true
dropChecks = true
dropMatesWhite = false
dropMatesBlack = true
startFen = 4k3/8/4K3/8/8/8/8/8[Q] w - - 0 1

[dropmate-split-black:chess]
pieceDrops = true
dropChecks = true
dropMatesWhite = false
dropMatesBlack = true
startFen = 8/8/8/8/8/4k3/8/4K3[q] b - - 0 1

[dropnodoubled-split-white:chess]
pieceDrops = true
dropNoDoubledWhite = p
startFen = 4k3/8/8/8/8/8/4P3/4K3[P] w - - 0 1

[dropnodoubled-split-black:chess]
pieceDrops = true
dropNoDoubledWhite = p
startFen = 4k3/4p3/8/8/8/8/8/4K3[p] b - - 0 1

[dropnodoubledcount-split-white:chess]
pieceDrops = true
dropNoDoubled = p
dropNoDoubledCountWhite = 2
startFen = 4k3/8/8/8/8/8/4P3/4K3[P] w - - 0 1

[dropnodoubledcount-split-black:chess]
pieceDrops = true
dropNoDoubled = p
dropNoDoubledCountWhite = 2
startFen = 4k3/4p3/8/8/8/8/8/4K3[p] b - - 0 1

[pathway-drop-rule]
maxRank = 6
maxFile = 6
immobile = p
pieceDrops = true
mustDrop = true
checking = false
doubleStep = false
castling = false
nMoveRule = 0
stalemateValue = win
pathwayDropRule = true
startFen = 6/6/6/6/6/6[Pp] w - - 0 1

[nana-drop-forms:chess]
maxRank = 3
maxFile = c
pieceDrops = true
customPiece1 = a:W
customPiece2 = b:F
customPiece3 = c:D
customPiece4 = d:N
dropPieceTypes = a:abcd;
dropRegionWhite = a1 b1 c1 a2 c2 a3 b3 c3
dropRegionBlack = a1 b1 c1 a2 c2 a3 b3 c3
startFen = 3/3/3[KkA] w - - 0 1
INI
tmp_ini="${FSX_TMP_INI}"

echo "drop regressions started"

run_perft() {
  local variant="$1"
  run_uci "$ENGINE" "$tmp_ini" "$variant" <<'UCI'
position startpos
go perft 1
UCI
}

run_display() {
  local variant="$1"
  local moves="$2"
  run_uci "$ENGINE" "$tmp_ini" "$variant" <<UCI
position startpos moves ${moves}
d
UCI
}

out=$(run_uci "$ENGINE" "$tmp_ini" borrow-slide <<'UCI'
position startpos
go perft 1
UCI
)
assert_contains "$out" "^A@a1,b1: 1$"

out=$(run_uci "$ENGINE" "$tmp_ini" borrow-slide <<'UCI'
position startpos moves A@a1,b1
d
UCI
)
assert_contains "$out" "Fen: 5/5/5/5/a4\\[] b - - 0 1"

out=$(run_uci "$ENGINE" "$tmp_ini" capture-to-hand-types-demo <<'UCI'
position fen r3k3/8/8/8/8/8/R3K3/8 w - - 0 1 moves a1a7
d
UCI
)
assert_contains "$out" "Fen: R3k3/8/8/8/8/8/4K3\\[R\\] b - - 0 1"

out=$(run_uci "$ENGINE" "$tmp_ini" capture-to-hand-types-demo <<'UCI'
position fen n3k3/8/8/8/8/8/R3K3/8 w - - 0 1 moves a1a7
d
UCI
)
assert_contains "$out" "Fen: R3k3/8/8/8/8/8/4K3\\[\\] b - - 0 1"

out=$(run_uci "$ENGINE" "$tmp_ini" capture-to-hand-types-demo <<'UCI'
position fen +l3k3/8/8/8/8/8/R3K3/8 w - - 0 1 moves a1a7
d
UCI
)
assert_contains "$out" "Fen: R3k3/8/8/8/8/8/4K3\\[L\\] b - - 0 1"

out=$(run_uci "$ENGINE" "$tmp_ini" owner-hand-capture <<'UCI'
position startpos moves b1b2
d
UCI
)
assert_contains "$out" "Sfen: 3/1A1/3 w Aa 2"

out=$(run_uci "$ENGINE" "$tmp_ini" owner-hand-eject <<'UCI'
position startpos moves A@a1,b1
d
UCI
)
assert_contains "$out" "Fen: AAAAa/5/5/5/A4\\[] b - - 0 1"

out=$(run_uci "$ENGINE" "$tmp_ini" pairdrop <<'UCI'
position fen 4k3/8/8/8/8/8/8/4K3[RR] w - - 0 1
go perft 1
UCI
)
assert_contains "$out" "^R@a4,h4: 1$"
assert_contains "$out" "^R@d4,e4: 1$"
assert_not_contains "$out" "^R@a4: 1$"

out=$(run_uci "$ENGINE" "$tmp_ini" pairdrop <<'UCI'
position fen 4k3/8/8/8/8/8/8/4K3[RR] w - - 0 1 moves R@a4,h4
d
UCI
)
assert_contains "$out" "Fen: 4k3/8/8/8/R6R/8/8/4K3\\[] b - - 0 1"

out=$(run_uci "$ENGINE" "$tmp_ini" shogi-pawn-drop-split-white <<'UCI'
position startpos
go perft 1
UCI
)
assert_not_contains "$out" "^P@c4: 1$"

out=$(run_uci "$ENGINE" "$tmp_ini" shogi-pawn-drop-split-black <<'UCI'
position startpos
go perft 1
UCI
)
assert_contains "$out" "^P@c2: 1$"

out=$(run_uci "$ENGINE" "$tmp_ini" shogi-pawn-drop-split-black <<'UCI'
setoption name Verbosity value 2
position startpos moves P@c2
go depth 1
UCI
)
assert_contains "$out" "info string adjudication reason checkmate result mate"
assert_contains "$out" "side_to_move white"

out=$(run_uci "$ENGINE" "$tmp_ini" capture-drop-control <<'UCI'
position startpos
go perft 1
UCI
)
assert_not_contains "$out" "^Q@e5: 1$"

out=$(run_uci "$ENGINE" "$tmp_ini" capture-drop <<'UCI'
position startpos
go perft 1
UCI
)
assert_contains "$out" "^Q@e5: 1$"

out=$(run_display "capture-drop" "Q@e5")
assert_contains "$out" "Fen: 4k3/8/8/4Q3/8/8/8/4K3\\[P\\] b - - 0 1"

out=$(run_uci "$ENGINE" "$tmp_ini" capture-drop-self <<'UCI'
position startpos
go perft 1
UCI
)
assert_contains "$out" "^Q@e4: 1$"

out=$(run_uci "$ENGINE" "$tmp_ini" dropcheck-split-white <<'UCI'
position startpos
go perft 1
UCI
)
assert_not_contains "$out" "^R@e7: 1$"

out=$(run_uci "$ENGINE" "$tmp_ini" dropcheck-split-black <<'UCI'
position startpos
go perft 1
UCI
)
assert_contains "$out" "^R@e2: 1$"

out=$(run_uci "$ENGINE" "$tmp_ini" dropmate-split-white <<'UCI'
position startpos
go perft 1
UCI
)
assert_not_contains "$out" "^Q@e7: 1$"

out=$(run_uci "$ENGINE" "$tmp_ini" dropmate-split-black <<'UCI'
position startpos
go perft 1
UCI
)
assert_contains "$out" "^Q@e2: 1$"

out=$(run_uci "$ENGINE" "$tmp_ini" dropnodoubled-split-white <<'UCI'
position startpos
go perft 1
UCI
)
assert_not_contains "$out" "^P@e4: 1$"

out=$(run_uci "$ENGINE" "$tmp_ini" dropnodoubled-split-black <<'UCI'
position startpos
go perft 1
UCI
)
assert_contains "$out" "^P@e5: 1$"

out=$(run_uci "$ENGINE" "$tmp_ini" dropnodoubledcount-split-white <<'UCI'
position startpos
go perft 1
UCI
)
assert_contains "$out" "^P@e4: 1$"

out=$(run_uci "$ENGINE" "$tmp_ini" dropnodoubledcount-split-black <<'UCI'
position startpos
go perft 1
UCI
)
assert_not_contains "$out" "^P@e5: 1$"

out=$(run_uci "$ENGINE" "$tmp_ini" pathway-drop-rule <<'UCI'
position fen 6/6/6/6/3p2/6[Pp] w - - 0 1
go perft 1
UCI
)
assert_not_contains "$out" "^P@c2: 1$"

out=$(run_uci "$ENGINE" "$tmp_ini" pathway-drop-rule <<'UCI'
position fen 6/6/6/2P3/3p2/6[Pp] w - - 0 1
go perft 1
UCI
)
assert_contains "$out" "^P@c2: 1$"

out=$(run_perft "nana-drop-forms")
assert_contains "$out" '^A@a1: 1$'
assert_contains "$out" '^B@a1: 1$'
assert_contains "$out" '^C@a1: 1$'
assert_contains "$out" '^D@a1: 1$'
assert_not_contains "$out" '@b2:'
assert_not_contains "$out" '^E@'

echo "drop regressions passed"
