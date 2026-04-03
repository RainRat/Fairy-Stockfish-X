#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
BIN=${1:-${REPO_ROOT}/src/stockfish}

TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

VARIANT_FILE="$TMPDIR/vlb-symbol.ini"
cat > "$VARIANT_FILE" <<'VAR'
[vlb-token-smoke:fairy]
maxRank = 5
maxFile = 5
pieceDrops = true
customPiece1 = a':W
pieceValueMg = a':321
startFen = 4k/5/5/5/A'3K[A'a'] w - - 0 1
VAR

OUT=$(
  printf 'setoption name VariantPath value %s\nsetoption name UCI_Variant value vlb-token-smoke\nposition startpos\nd\ngo perft 1\nquit\n' "$VARIANT_FILE" \
    | "$BIN" 2>&1
)

printf '%s\n' "$OUT"

[[ "$OUT" == *"variant vlb-token-smoke"* ]]
[[ "$OUT" == *"Fen: 4k/5/5/5/A'3K[A'a'] w - - 0 1"* ]]
[[ "$OUT" == *" | A' |"* ]]
[[ "$OUT" == *"A'@b1: 1"* ]]
[[ "$OUT" == *"Nodes searched: 27"* ]]
[[ "$OUT" != *"Invalid syntax"* ]]
[[ "$OUT" != *"Invalid piece character"* ]]
