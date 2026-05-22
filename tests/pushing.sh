#!/bin/bash

set -euo pipefail

ENGINE=${1:-./stockfish}
ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)

TMP_INI=$(mktemp)
trap 'rm -f "${TMP_INI}"' EXIT

cat > "${TMP_INI}" <<'INI'
[push-base:fairy]
maxFile = e
maxRank = 5
castling = false
checking = false
startFen = 5/5/5/5/5 w - - 0 1
rook = r
pushingStrength = r:5

[push-them:push-base]
pushingStrength = r:2
pushFirstColor = them
pushingRemoves = none

[push-us:push-them]
pushFirstColor = us

[push-shove:push-them]
pushingRemoves = shove

[push-stepwise-capture:push-base]
pushFirstColor = them
pushChainEnemyOnly = true
pushCaptureAgainstFriendlyBlocker = true
pushingRemoves = none
stepwisePushing = true

[push-stepwise-shove:push-base]
pushFirstColor = them
pushChainEnemyOnly = true
pushingRemoves = shove
stepwisePushing = true

[push-stepwise-no-blocker-capture:push-base]
pushFirstColor = them
pushChainEnemyOnly = true
pushCaptureAgainstFriendlyBlocker = false
pushingRemoves = none
stepwisePushing = true
INI

run_cmds() {
  local variant=$1
  local cmds=$2
  cat <<UCI | "${ENGINE}"
uci
setoption name VariantPath value ${TMP_INI}
setoption name UCI_Variant value ${variant}
isready
${cmds}
quit
UCI
}

echo "Testing push-them..."
out=$(run_cmds push-them "position fen 5/5/5/Rrr2/5 w - - 0 1
go perft 1")
echo "${out}" | grep -q "^a2b2: 1$"

out=$(run_cmds push-them "position fen 5/5/5/Rrrr1/5 w - - 0 1
go perft 1")
! echo "${out}" | grep -q "^a2b2: 1$"

echo "Testing push-us..."
out=$(run_cmds push-us "position fen 5/5/5/RR3/5 w - - 0 1
go perft 1")
echo "${out}" | grep -q "^a2b2: 1$"

echo "Testing push-shove..."
out=$(run_cmds push-shove "position fen 5/5/5/2Rrr/5 w - - 0 1 moves c2d2
d")
echo "${out}" | grep -q "Fen: 5/5/5/3Rr/5 b - - 0 1"

echo "Testing aries..."
out=$(cat <<EOF | "${ENGINE}"
uci
setoption name VariantPath value ${ROOT_DIR}/src/variants.ini
setoption name UCI_Variant value aries
isready
position fen 8/8/8/Rrrr4/8/8/8/8 w - - 0 1
go perft 1
quit
EOF
)
echo "${out}" | grep -q "^a5b5: 1$"
! echo "${out}" | grep -q "^a5c5: 1$"
! echo "${out}" | grep -q "^a5d5: 1$"
! echo "${out}" | grep -q "^a5e5: 1$"
echo "${out}" | grep -q "^Nodes searched: 8$"

echo "Testing push-stepwise-capture..."
out=$(run_cmds push-stepwise-capture "position fen 5/5/1R1r1/5/5 w - - 0 1 moves b3d3
d")
echo "${out}" | grep -o "Fen: [^ ]* [^ ]* [^ ]* [^ ]* [^ ]*" | grep -q "Fen: 5/5/3Rr/5/5 b - - 1"

out=$(run_cmds push-stepwise-capture "position fen 5/5/1R1rR/5/5 w - - 0 1 moves b3d3
d")
echo "${out}" | grep -o "Fen: [^ ]* [^ ]* [^ ]* [^ ]* [^ ]*" | grep -q "Fen: 5/5/3RR/5/5 b - - 0"

echo "Testing push-stepwise-shove..."
out=$(run_cmds push-stepwise-shove "position fen 5/5/1R1rr/5/5 w - - 0 1 moves b3d3
d")
echo "${out}" | grep -o "Fen: [^ ]* [^ ]* [^ ]* [^ ]* [^ ]*" | grep -q "Fen: 5/5/3Rr/5/5 b - - 0"

echo "Testing push-stepwise-no-blocker-capture..."
out=$(run_cmds push-stepwise-no-blocker-capture "position fen 5/5/1R1rR/5/5 w - - 0 1
go perft 1")
! echo "${out}" | grep -q "^b3d3: 1$"

echo "Testing control case..."
out=$(run_cmds push-stepwise-shove "position fen 5/5/R4/5/5 w - - 0 1
go perft 1")
echo "${out}" | grep -q "Nodes searched: 8"

echo "Testing perft round-trip (exercises undo_move)..."
out=$(run_cmds push-stepwise-capture "position fen 5/5/1R1rR/5/5 w - - 0 1
go perft 2")
echo "${out}" | grep -q "Nodes searched: 81"

echo "pushing ok"
