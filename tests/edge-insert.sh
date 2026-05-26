#!/bin/bash

set -euo pipefail

error() {
  echo "edge-insert test failed on line $1" >&2
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ENGINE="${1:-${SCRIPT_DIR}/../src/stockfish-large}"

source "${SCRIPT_DIR}/lib/uci.sh"

tmp_ini=$(mktemp)
trap 'rm -f "${tmp_ini}"' EXIT

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

# Corner insertion uses drop-style notation with an explicit insertion lane.
out=$(run_uci "${ENGINE}" "${tmp_ini}" edge-insert-demo <<'UCI'
position fen A4/5/5/5/5[AAAAAAAAA] w - - 0 1
go perft 1
UCI
)
assert_contains "$out" "^A@a4,b4: 1$"
assert_contains "$out" "^A@b5,b4: 1$"

# Top-edge insertion on the a-file pushes down the file.
out=$(run_uci "${ENGINE}" "${tmp_ini}" edge-insert-demo <<'UCI'
position fen A4/5/5/5/5[AAAAAAAAA] w - - 0 1 moves A@a4,b4
d
UCI
)
assert_contains "$out" "Fen: A4/A4/5/5/5\\[AAAAAAAA\\] b - - 0 1"

# Top-edge insertion on the b-file pushes across the top rank.
out=$(run_uci "${ENGINE}" "${tmp_ini}" edge-insert-demo <<'UCI'
position fen A4/5/5/5/5[AAAAAAAAA] w - - 0 1 moves A@b5,b4
d
UCI
)
assert_contains "$out" "Fen: AA3/5/5/5/5\\[AAAAAAAA\\] b - - 0 1"

# Plain drops must be rejected when edgeInsertOnly is enabled.
out=$(run_uci "${ENGINE}" "${tmp_ini}" edge-insert-demo <<'UCI'
position startpos moves A@a1
d
UCI
)
assert_contains "$out" "Fen: 5/5/5/5/5\\[AAAAAAAAAA\\] w - - 0 1"

echo "edge-insert tests passed"
