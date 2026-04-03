#!/bin/bash
# Regression test: captures-to-hand keeps exact promotion source piece type
# for variants with multiple promotion pawn types.

set -euo pipefail

error() {
  echo "crazyhouse-multi-pawn-promo testing failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
ENGINE="${1:-${REPO_ROOT}/src/stockfish}"

echo "crazyhouse-multi-pawn-promo testing started"

VARIANT_FILE=$(mktemp)
OUT_FILE=$(mktemp)
trap 'rm -f "$VARIANT_FILE" "$OUT_FILE"' EXIT

cat > "$VARIANT_FILE" <<'VAR'
[newvariant:crazyhouse]
promotionPawnTypes=pb
promotionPieceTypes=qn
VAR

cat <<CMDS | "$ENGINE" > "$OUT_FILE" 2>&1
uci
setoption name VariantPath value $VARIANT_FILE
setoption name UCI_Variant value newvariant
position fen r7/7P/8/8/8/8/8/k1K5 w - - 0 1 moves h7h8q a8h8
d
quit
CMDS

grep -Fq "Fen: 7r/8/8/8/8/8/8/k1K5[p] w - - 0 2" "$OUT_FILE"

echo "crazyhouse-multi-pawn-promo testing OK"