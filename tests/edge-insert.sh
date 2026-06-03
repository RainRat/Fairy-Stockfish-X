#!/bin/bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "edge-insert test"

load_inline_variants <<'INI'
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
tmp_ini="${FSX_TMP_INI}"

run_cmds() {
  run_uci "$ENGINE" "$tmp_ini" edge-insert-demo <<EOF
$1
EOF
}

# Corner insertion uses drop-style notation with an explicit insertion lane.
out=$(run_cmds "position fen A4/5/5/5/5[AAAAAAAAA] w - - 0 1
go perft 1")
echo "${out}" | grep -q "^A@a4,b4: 1$"
echo "${out}" | grep -q "^A@b5,b4: 1$"

# Top-edge insertion on the a-file pushes down the file.
out=$(run_cmds "position fen A4/5/5/5/5[AAAAAAAAA] w - - 0 1 moves A@a4,b4
d")
echo "${out}" | grep -q "Fen: A4/A4/5/5/5\\[AAAAAAAA\\] b - - 0 1"

# Top-edge insertion on the b-file pushes across the top rank.
out=$(run_cmds "position fen A4/5/5/5/5[AAAAAAAAA] w - - 0 1 moves A@b5,b4
d")
echo "${out}" | grep -q "Fen: AA3/5/5/5/5\\[AAAAAAAA\\] b - - 0 1"

# Plain drops must be rejected when edgeInsertOnly is enabled.
out=$(run_cmds "position startpos moves A@a1
d")
echo "${out}" | grep -q "Fen: 5/5/5/5/5\\[AAAAAAAAAA\\] w - - 0 1"

echo "edge-insert tests passed"
