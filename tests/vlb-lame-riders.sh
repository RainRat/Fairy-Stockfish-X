#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENGINE="${1:-$ROOT_DIR/src/stockfish-vlb}"

TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

VARIANT_FILE="$TMPDIR/vlb-lame-riders.ini"
cat > "$VARIANT_FILE" <<'VAR'
[vlb-lame-clear:fairy]
maxFile = p
maxRank = 16
customPiece1 = a:n{path:mid}D
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 12A2k/16/16/16/16/16/16/16/16/16/16/16/16/16/16/15K w - - 0 1

[vlb-lame-blocked:fairy]
maxFile = p
maxRank = 16
customPiece1 = a:n{path:mid}D
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 12A2k/12p3/16/16/16/16/16/16/16/16/16/16/16/16/16/15K w - - 0 1
VAR

run_variant() {
  local variant="$1"
  printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value %s\nposition startpos\ngo perft 1\nquit\n' \
    "$VARIANT_FILE" "$variant" | "$ENGINE" 2>&1
}

out=$(run_variant vlb-lame-clear)
if grep -q "exceeds build board limits" <<<"$out"; then
  echo "skip: VLB lame rider regression requires a very-large-board capable engine"
  exit 0
fi
if grep -q "No such variant" <<<"$out"; then
  echo "skip: VLB lame rider variant unavailable in this binary"
  exit 0
fi

grep -q "info string variant vlb-lame-clear files 16 ranks 16" <<<"$out"
grep -q "^m16m14: 1$" <<<"$out"
grep -q "^m16k16: 1$" <<<"$out"
grep -q "^m16o16: 1$" <<<"$out"
grep -q "Nodes searched: 6" <<<"$out"

out=$(run_variant vlb-lame-blocked)
grep -q "info string variant vlb-lame-blocked files 16 ranks 16" <<<"$out"
! grep -q "^m16m14:" <<<"$out"
grep -q "^m16k16: 1$" <<<"$out"
grep -q "^m16o16: 1$" <<<"$out"
grep -q "Nodes searched: 5" <<<"$out"

echo "VLB lame rider regression passed"
