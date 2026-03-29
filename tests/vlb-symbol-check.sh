#!/usr/bin/env bash
set -euo pipefail

BIN=${1:-/home/chris/Fairy-Stockfish-X/src/stockfish}

TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

VARIANT_FILE="$TMPDIR/vlb-symbol-check.ini"
cat > "$VARIANT_FILE" <<'VAR'
[vlb-token-check:fairy]
maxRank = 5
maxFile = 5
customPiece1 = a':W
customPiece2 = a":F
startFen = 4k/5/5/5/A'2A"K w - - 0 1
VAR

OUT=$(
  printf 'setoption name VariantPath value %s\nquit\n' "$VARIANT_FILE" \
    | "$BIN" 2>&1
)

printf '%s\n' "$OUT"

[[ "$OUT" != *"Ambiguous piece character"* ]]
[[ "$OUT" != *"Ambiguous piece symbol"* ]]
