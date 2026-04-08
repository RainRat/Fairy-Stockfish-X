#!/bin/bash

set -euo pipefail

error() {
  echo "edge-insert test failed on line $1" >&2
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./src/stockfish-large}

tmp_ini=$(mktemp)
cat > "${tmp_ini}" <<'INI'
[edge-insert-demo:chess]
maxRank = 5
maxFile = e
pieceToCharTable = -
king = -
customPiece1 = a:mW
startFen = 5/5/5/5/5[AAAAAAAAAA] w - - 0 1
pieceDrops = true
mustDrop = true
checking = false
pushingStrength = a:5
pushFirstColor = either
pushingRemoves = shove
edgeInsertOnly = true
edgeInsertTypes = a
edgeInsertRegionWhite = a5 b5 c5 d5 e5 a1 a2 a3 a4 a5
edgeInsertFromWhite = top left
dropRegionWhite = a5 b5 c5 d5 e5 a1 a2 a3 a4 a5
INI

run_cmds() {
  cat <<EOF | "${ENGINE}" 2>/dev/null
uci
setoption name VariantPath value ${tmp_ini}
$1
quit
EOF
}

# Corner insertion uses drop-style notation with an explicit insertion lane.
out=$(run_cmds "setoption name UCI_Variant value edge-insert-demo
position fen A4/5/5/5/5[AAAAAAAAA] w - - 0 1
go perft 1")
echo "${out}" | grep -q "^A@a4,b4: 1$"
echo "${out}" | grep -q "^A@b5,b4: 1$"

# Top-edge insertion on the a-file pushes down the file.
out=$(run_cmds "setoption name UCI_Variant value edge-insert-demo
position fen A4/5/5/5/5[AAAAAAAAA] w - - 0 1 moves A@a4,b4
d")
echo "${out}" | grep -q "Fen: A4/A4/5/5/5\\[AAAAAAAA\\] b - - 0 1"

# Top-edge insertion on the b-file pushes across the top rank.
out=$(run_cmds "setoption name UCI_Variant value edge-insert-demo
position fen A4/5/5/5/5[AAAAAAAAA] w - - 0 1 moves A@b5,b4
d")
echo "${out}" | grep -q "Fen: AA3/5/5/5/5\\[AAAAAAAA\\] b - - 0 1"

# Plain drops must be rejected when edgeInsertOnly is enabled.
out=$(run_cmds "setoption name UCI_Variant value edge-insert-demo
position startpos moves A@a1
d")
echo "${out}" | grep -q "Fen: 5/5/5/5/5\\[AAAAAAAAAA\\] w - - 0 1"

rm -f "${tmp_ini}"
echo "edge-insert tests passed"
