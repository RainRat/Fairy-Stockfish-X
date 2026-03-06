#!/bin/bash

set -euo pipefail

error() {
  echo "new-variants smoke test failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./stockfish}
VARIANT_PATH=${2:-variants.ini}

run_cmds() {
  cat <<EOF | "${ENGINE}"
uci
setoption name VariantPath value ${VARIANT_PATH}
$1
quit
EOF
}

echo "new variants smoke testing started"

# 1) Hasami: orthogonal sandwich should capture the middle piece.
out=$(run_cmds "setoption name UCI_Variant value hasami
position fen 9/9/9/9/9/9/9/R1rR5/9 w - - 0 1 moves a2b2
d")
echo "${out}" | grep -q "Fen: 9/9/9/9/9/9/9/1R1R5/9 b - - 1 1"

# 2) Hasami: edge alone is not hostile; moving away must not capture.
out=$(run_cmds "setoption name UCI_Variant value hasami
position fen 9/9/9/9/9/9/9/Rr7/9 w - - 0 1 moves a2a1
d")
echo "${out}" | grep -q "Fen: 9/9/9/9/9/9/9/1r7/R8 b - - 1 1"

# 3) Achi: pre-connected line is immediate game end (no legal moves).
out=$(run_cmds "setoption name UCI_Variant value achi
position fen PPP/3/3[PPPPpppp] b - - 0 1
go perft 1")
echo "${out}" | grep -q "Nodes searched: 0"

# 4) Achi: non-terminal filled setup still yields legal drops.
out=$(run_cmds "setoption name UCI_Variant value achi
position fen PpP/pPp/3[PpppPPPP] w - - 0 1
go perft 1")
echo "${out}" | grep -q "Nodes searched: 3"

# 5) Checkless: king capture is legal (checks are disabled by variant).
out=$(run_cmds "setoption name UCI_Variant value checkless
position fen 4k3/8/8/8/8/8/4Q3/4K3 w - - 0 1
go perft 1")
echo "${out}" | grep -q "^e2e8: 1$"

# 6) Tablut split: edge-escape should end immediately, corner-escape should not.
out=$(run_cmds "setoption name UCI_Variant value tablut
position fen 4K4/9/9/9/4r4/9/9/9/9 b - - 0 1
go perft 1")
echo "${out}" | grep -q "Nodes searched: 0"

out=$(run_cmds "setoption name UCI_Variant value tablut-corner-escape
position fen 4K4/9/9/9/4r4/9/9/9/9 b - - 0 1
go perft 1")
echo "${out}" | grep -q "Nodes searched: 15"

# 7) Tablut split: throne-adjacent king strength changes capture outcome.
out=$(run_cmds "setoption name UCI_Variant value tablut
position fen 9/9/9/9/3K5/2r6/9/9/9 b - - 0 1 moves c4c5
go perft 1")
echo "${out}" | grep -q "Nodes searched: 0"

out=$(run_cmds "setoption name UCI_Variant value tablut-throne-adjacent-strong
position fen 9/9/9/9/3K5/2r6/9/9/9 b - - 0 1 moves c4c5
go perft 1")
echo "${out}" | grep -q "Nodes searched: 12"

# 8) Neutreeko: max-distance move completes a line and ends the game.
out=$(run_cmds "setoption name UCI_Variant value neutreeko
position fen 5/3N1/5/1N3/N4 w - - 0 1 moves d4c3
go perft 1")
echo "${out}" | grep -q "Nodes searched: 0"

# 9) Forced en passant: when EP is legal, ordinary king moves are forbidden.
out=$(run_cmds "setoption name UCI_Variant value forced-en-passant
position fen 4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1
go perft 1")
echo "${out}" | grep -q "^e5d6: 1$"
echo "${out}" | grep -q "Nodes searched: 1"

# 10) Eurasian: entering the optional promotion band without reserve is still legal.
out=$(run_cmds "setoption name UCI_Variant value eurasian
position fen 4k5/10/P9/10/10/10/10/10/10/5K4 w - - 0 1
go perft 1")
echo "${out}" | grep -q "^a8a9: 1$"

# 11) Eurasian: entering the last rank without a reserve promotion is forbidden.
out=$(run_cmds "setoption name UCI_Variant value eurasian
position fen 4k5/P9/10/10/10/10/10/10/10/5K4 w - - 0 1
go perft 1")
! echo "${out}" | grep -q "^a9a10"

# 12) Eurasian: a matching reserve piece enables the last-rank promotion.
out=$(run_cmds "setoption name UCI_Variant value eurasian
position fen 4k5/P9/10/10/10/10/10/10/10/5K4[Q] w - - 0 1
go perft 1")
echo "${out}" | grep -q "^a9a10q: 1$"

# 13) Fatal giveaway: non-pawn capturer dies, pawns survive captures.
out=$(run_cmds "setoption name UCI_Variant value fatal-giveaway
position fen 4k3/8/8/4p3/4R3/8/8/8 w - - 0 1 moves e4e5
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/4\\^3/8/8/8/8 b - - 0 1"
out=$(run_cmds "setoption name UCI_Variant value fatal-giveaway
position fen 4k3/8/8/3p4/4P3/8/8/8 w - - 0 1 moves e4d5
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/3P4/8/8/8/8 b - - 0 1"

# 14) Kamikaze giveaway: any capturer is removed after capture.
out=$(run_cmds "setoption name UCI_Variant value kamikaze-giveaway
position fen 4k3/8/8/3p4/4P3/8/8/8 w - - 0 1 moves e4d5
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/8/8/8/8/8 b - - 0 1"

# 15) Kamikaze: plain chess family, capturer is removed.
out=$(run_cmds "setoption name UCI_Variant value kamikaze
position fen 4k3/8/8/3p4/4P3/8/8/8 w - - 0 1 moves e4d5
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/8/8/8/8/8 b - - 0 1"

# 16) Fatal giveaway: dead squares can be captured as neutral blockers.
out=$(run_cmds "setoption name UCI_Variant value fatal-giveaway
position fen 4k3/8/8/4\\^3/8/8/8/4Q3 w - - 0 1 moves e1e5
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/4Q3/8/8/8/8 b - - 0 1"

# 17) Progressive: after Black's first move, White must pass before Black's second move.
out=$(run_cmds "setoption name UCI_Variant value progressive
position startpos moves e2e4 e7e5
go perft 1")
echo "${out}" | grep -q "^e1e1: 1$"

out=$(run_cmds "setoption name UCI_Variant value progressive
position startpos moves e2e4 e7e5 e1e1
go perft 1")
echo "${out}" | grep -q "Nodes searched: 29"

# 18) Hindustani baseline: no pawn double-step.
out=$(run_cmds "setoption name UCI_Variant value hindustani
position startpos
go perft 1")
! echo "${out}" | grep -q "^e2e4:"

# 19) Hindustani baseline: if all promotion targets are at cap, promotion is forbidden.
out=$(run_cmds "setoption name UCI_Variant value hindustani
position fen 4k3/3P4/8/8/8/8/RNBQKBNR/R6R w - - 0 1
go perft 1")
! echo "${out}" | grep -q "^d7d8"

# 20) Hindustani baseline: if queen is below cap, central-file promotion to queen is legal.
out=$(run_cmds "setoption name UCI_Variant value hindustani
position fen 4k3/3P4/8/8/8/8/RNB1KBNR/R6R w - - 0 1
go perft 1")
echo "${out}" | grep -q "^d7d8q: 1$"
! echo "${out}" | grep -q "^d7d8r:"

# 21) Gala: opening drop phase starts from pockets on own half (white has 10 choices).
out=$(run_cmds "setoption name UCI_Variant value gala
position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched: 10"

# 22) Gala: custodial capture by orthogonal sandwich.
out=$(run_cmds "setoption name UCI_Variant value gala
position fen 5/5/1D1dD/5/5 w - - 0 1 moves b3c3
d")
echo "${out}" | grep -q "Fen: 5/5/2D1D/5/5 b - - 1 1"

echo "new variants smoke testing OK"
