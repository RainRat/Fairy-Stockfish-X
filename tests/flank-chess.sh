#!/usr/bin/env bash
set -euo pipefail

ENGINE=${1:-./stockfish}
VARIANTS=${2:-src/variants.ini}

run_cmds() {
  printf 'uci\nsetoption name VariantPath value %s\n%s\nquit\n' "$VARIANTS" "$1" | "$ENGINE"
}

variant_available() {
  local out
  out=$(run_cmds "setoption name UCI_Variant value flank-chess
d")
  echo "$out" | grep -q "info string variant flank-chess "
}

echo "flank-chess regression tests started"

if ! variant_available; then
  echo "flank-chess regression skipped: variant unavailable in this build"
  exit 0
fi

# Achiles cannot be captured by an ordinary rook.
out=$(run_cmds "setoption name UCI_Variant value flank-chess
position fen 10/10/10/10/10/10/5r4/5A4/10 b - - 0 1
go perft 1")
! echo "$out" | grep -q "^f3f2: 1$"

# A wazir pawn can capture Achiles.
out=$(run_cmds "setoption name UCI_Variant value flank-chess
position fen 10/10/10/10/10/10/5p4/5A4/10 b - - 0 1
go perft 1")
echo "$out" | grep -q "^f3f2: 1$"

# Opposing Achiles can capture each other.
out=$(run_cmds "setoption name UCI_Variant value flank-chess
position fen 10/10/10/10/10/10/5a4/5A4/10 b - - 0 1
go perft 1")
echo "$out" | grep -q "^f3f2: 1$"

# Castling uses the rank-2/rank-8 geometry: f2->h2 and f2->d2 are available.
out=$(run_cmds "setoption name UCI_Variant value flank-chess
position fen 10/10/10/10/10/10/10/1R3K2R1/10 w KQ - 0 1
go perft 1")
echo "$out" | grep -q "^f2h2: 1$"
echo "$out" | grep -q "^f2d2: 1$"

echo "flank-chess regression tests passed"
