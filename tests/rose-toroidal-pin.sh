#!/bin/bash

set -euo pipefail

error() {
  echo "rose-toroidal-pin regression failed on line $1"
  [[ -n "${TMP_VARIANT_PATH:-}" ]] && rm -f "${TMP_VARIANT_PATH}"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./stockfish}

TMP_VARIANT_PATH=$(mktemp /tmp/fsx-rose-toroidal-XXXXXX.ini)
# Standard 8x8 board, toroidal.
cat >"${TMP_VARIANT_PATH}" <<'INI'
[rose-toroidal-8x8:chess]
toroidal = true
customPiece1 = a:rose
pieceToCharTable = A:a
# A1 King at (0,0)
# Step {df=1, dr=2}
# Path from G5 (6,4):
# Step 1: {1,2} -> (7,6) H7.
# Step 2: {1,2} -> (0,0) A1.
# So G5 attacks A1 via H7.
# Rank 7: ....... (8)
# Rank 6: .......P (7P) -> H7 is white pawn
# Rank 5: ......a. (6a1) -> G5 is black rose
# Rank 1: K....... (K7)
startFen = 8/8/7P/6a1/8/8/8/K7 w - - 0 1
INI

get_legal_moves() {
  local variant="$1"
  "${ENGINE}" <<CMDS
uci
setoption name VariantPath value ${TMP_VARIANT_PATH}
setoption name UCI_Variant value ${variant}
position startpos
go perft 1
quit
CMDS
}

echo "rose-toroidal-pin test started"

moves=$(get_legal_moves rose-toroidal-8x8)
echo "MOVES START"
echo "${moves}"
echo "MOVES END"

# If h7 is pinned to a1 by rose on g5 via wrapping:
# (G5 -> H7 -> A1)
# H7 cannot move. h7h8 should be illegal.
if echo "${moves}" | grep -q "h7h8"; then
  echo "FAILURE: Pawn on h7 is NOT recognized as pinned (can move to h8)"
  exit 1
fi

rm -f "${TMP_VARIANT_PATH}"
unset TMP_VARIANT_PATH

echo "rose-toroidal-pin test passed"
