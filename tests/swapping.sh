#!/bin/bash

set -euo pipefail

error() {
  echo "swapping regression failed on line $1" >&2
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ENGINE="${1:-${SCRIPT_DIR}/../src/stockfish}"
VARIANT_PATH="${2:-${SCRIPT_DIR}/../src/variants.ini}"

TMP_INI=$(mktemp)
trap 'rm -f "${TMP_INI}"' EXIT

cat > "${TMP_INI}" <<'INI'
[swap-basic:fairy]
maxFile = e
maxRank = 5
castling = false
checking = false
king = -
pieceToCharTable = -
customPiece1 = a:mW
customPiece2 = b:mW
adjacentSwapMoveTypes = a
adjacentSwapRequiresEmptyNeighbor = true
swapNoImmediateReturn = true
startFen = 5/5/5/5/5 w - - 0 1
INI

run_cmds() {
  local path=$1
  local variant=$2
  local cmds=$3
  cat <<CMD_EOF | "${ENGINE}"
uci
setoption name VariantPath value ${path}
setoption name UCI_Variant value ${variant}
isready
${cmds}
quit
CMD_EOF
}

out=$(run_cmds "${TMP_INI}" swap-basic "position fen 5/5/2Ab1/5/5 w - - 0 1
go perft 1")
echo "${out}" | grep -q "^c3c2: 1$"
echo "${out}" | grep -q "^c3b3: 1$"
echo "${out}" | grep -q "^c3c4: 1$"
echo "${out}" | grep -q "^c3d3s: 1$"

out=$(run_cmds "${TMP_INI}" swap-basic "position fen 5/2a2/1aAb1/2a2/5 w - - 0 1
go perft 1")
! echo "${out}" | grep -q "^c3d3s: 1$"

out=$(run_cmds "${TMP_INI}" swap-basic "position fen 5/5/2Ab1/5/5 w - - 0 1 moves c3d3s
go perft 1")
! echo "${out}" | grep -q "^d3c3s: 1$"

out=$(run_cmds "${VARIANT_PATH}" lewthwaite-swap "position startpos
go perft 1")
grep -q "Nodes searched:" <<<"$out"
! echo "${out}" | grep -q "s: 1$"

# Regression: SWAP gives check
# White king a1, Black king e5, White piece A (Rook) c1, Black piece b d1.
# Swap c1d1s -> A moves to d1, checks king at e5.
cat > "${TMP_INI}" <<'INI'
[swap-check:fairy]
maxFile = e
maxRank = 5
king = k
customPiece1 = a:mR
customPiece2 = b:mR
adjacentSwapMoveTypes = a
startFen = 5/5/5/5/5 w - - 0 1
INI
out=$(run_cmds "${TMP_INI}" swap-check "position fen 4k3/5/5/5/2Ab1 w - - 0 1
go perft 1")
echo "${out}" | grep -q "^c1d1s: 1$"

# Regression: SWAP legal (king safety)
# White king d2, Black Rook d5. Piece A at c1, piece b at d1.
# Swap c1d1s -> A moves to d1, b moves to c1.
# King at d2 is still in check from Rook d5 (unblocked by A/b swap).
# So c1d1s should be illegal.
out=$(run_cmds "${TMP_INI}" swap-check "position fen 3r2/5/5/3K4/5/2Ab1 w - - 0 1
go perft 1")
! echo "${out}" | grep -q "c1d1s: 1"

# Regression: rule50 reset on Pawn swap
cat > "${TMP_INI}" <<'INI'
[swap-pawn:fairy]
maxFile = e
maxRank = 5
king = -
adjacentSwapMoveTypes = p
startFen = 5/5/5/5/5 w - - 0 1
INI
out=$(run_cmds "${TMP_INI}" swap-pawn "position fen 5/5/2Pb1/5/5 w - - 40 1 moves c3d3s
d")
echo "${out}" | grep -q "Fen: 5/5/2bP1/5/5 b - - 0 1"

echo "swapping ok"
