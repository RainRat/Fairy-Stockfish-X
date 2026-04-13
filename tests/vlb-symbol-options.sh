#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
BIN=${1:-${SCRIPT_DIR}/../src/stockfish}

TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

VARIANT_FILE="$TMPDIR/vlb-symbol-options.ini"
cat > "$VARIANT_FILE" <<'VAR'
[vlb-token-options:fairy]
maxRank = 5
maxFile = 5
pieceDrops = true
customPiece1 = z':W
customPiece2 = y':F
pieceValueMg = z':321 y':222
pieceValueEg = z':111 y':112
piecePoints = z':3 y':4
promotionLimit = z':1 y':2
promotedPieceType = p:z' z':q
moveMorphPieceType = n:z' y':-
dropPieceTypes = k:z' y'; z':-; y':z'
priorityDropTypes = z' y'
pushingStrength = z':2 y':3
virtualDropLimit = z':2 y':1
captureForbidden = z':y'
connectN = 3
connectPieceTypes = z' y'
connectGoalByType = true
connectPieceGoalWhite = z' y'
connectPieceGoalBlack = y' z'
startFen = 4k/5/5/5/Z'NY'1K[Z'Y'] w - - 0 1
VAR

OUT=$(
  printf 'setoption name VariantPath value %s\nsetoption name UCI_Variant value vlb-token-options\nposition startpos\nd\ngo perft 1\nquit\n' "$VARIANT_FILE" \
    | "$BIN" 2>&1
)

printf '%s\n' "$OUT"

[[ "$OUT" == *"variant vlb-token-options"* ]]
[[ "$OUT" == *"Fen: 4k/5/5/5/Z'NY'1K[Y'Z'] w - - 0 1"* ]]
[[ "$OUT" == *"Z'"* ]]
[[ "$OUT" == *"Y'"* ]]
[[ "$OUT" == *"Nodes searched:"* ]]
[[ "$OUT" != *"Invalid syntax"* ]]
[[ "$OUT" != *"Invalid piece type"* ]]
