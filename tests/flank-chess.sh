#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ENGINE="${1:-${SCRIPT_DIR}/../src/stockfish}"
VARIANTS="${2:-${SCRIPT_DIR}/../src/variants.ini}"
source "${SCRIPT_DIR}/lib/uci.sh"

run_cmds() {
  run_uci "$ENGINE" "$VARIANTS" flank-chess <<EOF
$1
EOF
}

variant_available() {
  local out
  out=$(printf 'uci\nquit\n' | uci_timeout "$ENGINE")
  grep -q ' var flank-chess ' <<<"$out"
}

echo "flank-chess regression tests started"

if ! variant_available; then
  echo "flank-chess regression skipped: variant unavailable in this build"
  exit 0
fi

# Source-backed setup: home rows on ranks 2/8, soldier rows on 3/7, empty rim on 1/9.
out=$(run_cmds "setoption name UCI_Variant value flank-chess
position startpos
d")
assert_contains "$out" "Fen: 10/1rzsqkszr1/1ppppappp1/10/10/10/1PPPPAPPP1/1RZSQKSZR1/10 w KQkq - 0 1"

# Achiles cannot be captured by an ordinary rook.
out=$(run_cmds "setoption name UCI_Variant value flank-chess
position fen 10/10/10/10/10/10/5r4/5A4/10 b - - 0 1
go perft 1")
assert_not_contains "$out" "^f3f2: 1$"

# A wazir pawn can capture Achiles.
out=$(run_cmds "setoption name UCI_Variant value flank-chess
position fen 10/10/10/10/10/10/5p4/5A4/10 b - - 0 1
go perft 1")
assert_contains "$out" "^f3f2: 1$"

# Opposing Achiles can capture each other.
out=$(run_cmds "setoption name UCI_Variant value flank-chess
position fen 10/10/10/10/10/10/5a4/5A4/10 b - - 0 1
go perft 1")
assert_contains "$out" "^f3f2: 1$"

# Castling uses the rank-2/rank-8 geometry: f2->h2 and f2->d2 are available.
out=$(run_cmds "setoption name UCI_Variant value flank-chess
position fen 10/10/10/10/10/10/10/1R3K2R1/10 w KQ - 0 1
go perft 1")
assert_contains "$out" "^f2h2: 1$"
assert_contains "$out" "^f2d2: 1$"

echo "flank-chess regression tests passed"
