#!/usr/bin/env bash

set -euo pipefail

error() {
  echo "pulling regression failed on line $1" >&2
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ENGINE="${1:-${SCRIPT_DIR}/../src/stockfish}"

TMP_INI=$(mktemp)
trap 'rm -f "${TMP_INI}"' EXIT

cat > "${TMP_INI}" <<'INI'
[pull-basic:fairy]
maxFile = e
maxRank = 5
castling = false
checking = false
king = -
pieceToCharTable = -
customPiece1 = a:mW
customPiece2 = b:mW
customPiece3 = c:mW
pullingStrength = a:3 b:1 c:3
startFen = 5/5/5/5/5 w - - 0 1

[pull-evasion:fairy]
maxFile = e
maxRank = 5
pieceToCharTable = K...A...R...k...b...r...
king = k
customPiece1 = a:mW
pullingStrength = a:3 r:1
startFen = 5/5/5/5/5 w - - 0 1

[pull-chess:fairy]
pullingStrength = q:9 r:5 b:3 n:3 p:1
INI

run_cmds() {
  local variant=$1
  local cmds=$2
  cat <<EOF | "${ENGINE}" 2>/dev/null
uci
setoption name VariantPath value ${TMP_INI}
setoption name UCI_Variant value ${variant}
isready
${cmds}
quit
EOF
}

echo "Testing pull-basic..."
out=$(run_cmds pull-basic "position fen 5/5/2b2/2A2/5 w - - 0 1
go perft 1")
echo "${out}" | grep -q "^c2d2: 1$"
echo "${out}" | grep -q "^c2d2,c3: 1$"

echo "Testing pull-evasion..."
# white king at c3, black rook at c4 (checks king)
# white piece A at d4 (mW, pulls r from c4 to d4 by moving d4d3)
out=$(run_cmds pull-evasion "position fen 5/2rA1/2K2/5/5 w - - 0 1
go perft 1")
echo "${out}" | grep -q "^d4d3,c4: 1$"

echo "Testing pull-chess FEN round-trip..."
# Move: e4d4,e5 pulls enemy pawn from e5 to e4 while queen moves from e4 to d4.
out=$(run_cmds pull-chess "position fen rnbqkbnr/pppp1ppp/8/4p3/4Q3/8/PPPP1PPP/RNBK1BNR w KQkq - 0 1 moves e4d4,e5
d")
echo "${out}" | grep -q "Fen: rnbqkbnr/pppp1ppp/8/8/3Qp3/8/PPPP1PPP/RNBK1BNR b KQkq - 1 1"

echo "pulling ok"
