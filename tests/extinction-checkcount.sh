#!/bin/bash
# Regression test: checkCounting must decrement on pseudo-royal checks

set -euo pipefail

error() {
  echo "extinction-checkcount testing failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./stockfish}

echo "extinction-checkcount testing started"

VARIANT_FILE=$(mktemp)
OUT_FILE=$(mktemp)
trap 'rm -f "$VARIANT_FILE" "$OUT_FILE"' EXIT

cat > "$VARIANT_FILE" <<'VAR'
[test_extinction_check_count]
knight = n
queen = q
king = -
castling = false
extinctionValue = loss
extinctionPieceTypes = *
extinctionPseudoRoyal = true
checkCounting = true
startFen = 4n3/8/8/8/8/8/8/3Q4 w - - 9+9 0 1
VAR

cat <<CMDS | "$ENGINE" > "$OUT_FILE" 2>&1
uci
setoption name VariantPath value $VARIANT_FILE
setoption name UCI_Variant value test_extinction_check_count
position startpos moves d1e1
d
quit
CMDS

grep -Fq "Fen: 4n3/8/8/8/8/8/8/4Q3 b - - 8+9 1 1" "$OUT_FILE"

echo "extinction-checkcount testing OK"
