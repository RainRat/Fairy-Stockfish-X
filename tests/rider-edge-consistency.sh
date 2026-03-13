#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")/../src"

tmp_ini=$(mktemp)
trap 'rm -f "$tmp_ini"' EXIT

cat > "$tmp_ini" <<'INI'
[nr-edge:chess]
maxRank = 5
maxFile = 5
castling = false
doubleStep = false
customPiece1 = a:NN
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k

[dabbaba-edge:chess]
maxRank = 5
maxFile = 5
castling = false
doubleStep = false
customPiece1 = a:DD
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k

[alfil-edge:chess]
maxRank = 5
maxFile = 5
castling = false
doubleStep = false
customPiece1 = a:AA
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k

[griffon-edge:chess]
maxRank = 5
maxFile = 5
castling = false
doubleStep = false
customPiece1 = a:O
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k

[manticore-edge:chess]
maxRank = 5
maxFile = 5
castling = false
doubleStep = false
customPiece1 = a:M
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
INI

run_searchmove() {
  local variant="$1"
  local fen="$2"
  local move="$3"
  printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value %s\nposition fen %s\ngo depth 1 searchmoves %s\nquit\n' \
    "$tmp_ini" "$variant" "$fen" "$move" | ./stockfish
}

expect_legal() {
  local variant="$1"
  local fen="$2"
  local move="$3"
  local out
  out=$(run_searchmove "$variant" "$fen" "$move")
  grep -Fq "bestmove $move" <<<"$out"
}

expect_illegal() {
  local variant="$1"
  local fen="$2"
  local move="$3"
  local out
  out=$(run_searchmove "$variant" "$fen" "$move")
  grep -Fq "bestmove (none)" <<<"$out"
}

# Edge-trimmed nightrider rays must stop cleanly on a 5x5 board.
expect_legal nr-edge "5/5/4K/5/a1R2 w - - 0 1" "c1c2"
expect_illegal nr-edge "5/5/4K/5/a1R2 w - - 0 1" "c1b1"

# Fixed-step riders must only expose landing-square paths at the edge.
expect_legal dabbaba-edge "5/5/5/2R2/a3K w - - 0 1" "c2c1"
expect_illegal dabbaba-edge "5/5/5/2R2/a3K w - - 0 1" "c2b2"

expect_legal alfil-edge "4K/5/5/2R2/a4 w - - 0 1" "c2c3"
expect_illegal alfil-edge "4K/5/5/2R2/a4 w - - 0 1" "c2b2"

# Bent-slider path reconstruction must not fabricate off-board continuations.
expect_legal griffon-edge "2K2/a4/5/1R3/5 w - - 0 1" "b2b5"
expect_illegal griffon-edge "2K2/a4/5/1R3/5 w - - 0 1" "b2b4"

expect_legal manticore-edge "1K3/2A2/a4/5/5 w - - 0 1" "c4a3"
expect_illegal manticore-edge "1K3/2A2/a4/5/5 w - - 0 1" "c4b4"

echo "rider-edge-consistency test OK"
