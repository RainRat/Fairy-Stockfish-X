#!/usr/bin/env bash

set -euo pipefail

error() {
  echo "drop regression failed on line $1" >&2
  exit 1
}
trap 'error ${LINENO}' ERR

source "$(dirname "${BASH_SOURCE[0]}")/lib/uci.sh"

ENGINE="${1:-$(default_engine)}"

init_tmp_ini
cat >"${FSX_TMP_INI}" <<'INI'
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

run_perft() {
  local variant="$1"
  run_uci "$ENGINE" "$FSX_TMP_INI" "$variant" <<'UCI'
position startpos
go perft 1
UCI
}

run_display() {
  local variant="$1"
  local moves="$2"
  run_uci "$ENGINE" "$FSX_TMP_INI" "$variant" <<UCI
position startpos moves ${moves}
d
UCI
}

echo "drop regression tests started"

out=$(run_perft "capture-drop-control")
assert_not_contains "$out" "^Q@e5: 1$"

out=$(run_perft "capture-drop")
assert_contains "$out" "^Q@e5: 1$"

out=$(run_display "capture-drop" "Q@e5")
assert_contains_literal "$out" "Fen: 4k3/8/8/4Q3/8/8/8/4K3[P] b - - 0 1"

out=$(run_perft "capture-drop-self")
assert_contains "$out" "^Q@e4: 1$"

out=$(run_perft "dropcheck-split-white")
assert_not_contains "$out" "^R@e7: 1$"

out=$(run_perft "dropcheck-split-black")
assert_contains "$out" "^R@e2: 1$"

out=$(run_perft "dropmate-split-white")
assert_not_contains "$out" "^Q@e7: 1$"

out=$(run_perft "dropmate-split-black")
assert_contains "$out" "^Q@e2: 1$"

out=$(run_perft "dropnodoubled-split-white")
assert_not_contains "$out" "^P@e4: 1$"

out=$(run_perft "dropnodoubled-split-black")
assert_contains "$out" "^P@e5: 1$"

out=$(run_perft "dropnodoubledcount-split-white")
assert_contains "$out" "^P@e4: 1$"

out=$(run_perft "dropnodoubledcount-split-black")
assert_not_contains "$out" "^P@e5: 1$"

out=$(run_uci "$ENGINE" "$FSX_TMP_INI" pathway-drop-rule <<'UCI'
position fen 6/6/6/6/3p2/6[Pp] w - - 0 1
go perft 1
UCI
)
assert_not_contains "$out" "^P@c2: 1$"

out=$(run_uci "$ENGINE" "$FSX_TMP_INI" pathway-drop-rule <<'UCI'
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

echo "drop regression tests passed"
