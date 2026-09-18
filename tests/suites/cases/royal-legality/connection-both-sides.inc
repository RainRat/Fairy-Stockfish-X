#!/usr/bin/env bash
# Connection adjudication must evaluate both sides when the mover's move can
# complete the opponent's connection. These fixtures pin the two gate-open
# families (see Variant::connectionMoverOnly): mechanics that relocate or
# convert opponent pieces (here: swap moves), and count-derived goals where
# even removals complete (here: connect-all groups). Each tie pairs with a
# control where the opponent's line stays incomplete and play continues.

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/../../../../tests/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "connection both-sides regression"

load_inline_variants <<'INI'
[swapconn:chess]
checking = false
castling = false
customPiece1 = a:W
customPiece2 = b:B
adjacentSwapMoveTypes = a
adjacentSwapTargetTypes = b
connectN = 3
connectPieceTypes = b
connectHorizontal = true
connectVertical = false
connectDiagonal = false

[groupcap:chess]
connectGroup = -1
INI
tmp_ini="${FSX_TMP_INI}"

assert_decisive_end() {
  local out="$1"
  assert_contains "$out" "info string adjudication reason game_end"
  assert_contains "$out" "result mate"
  assert_contains "$out" "^bestmove \(none\)$"
}

assert_continues() {
  local out="$1"
  assert_not_contains "$out" "adjudication reason game_end"
  assert_not_contains "$out" "^bestmove \(none\)$"
}

echo "connection both-sides regression tests started"

# White swaps b1/b2, moving the black stone to b1 and completing black's
# a1-b1-c1 line: black wins even though white just moved.
out=$(run_uci "$ENGINE" "$tmp_ini" swapconn <<'UCI'
setoption name Verbosity value 2
position fen 4k3/8/8/8/8/8/1b6/bAb1K3 w - - 0 1 moves b1b2s
go depth 1
UCI
)
assert_decisive_end "$out"

# Same swap without the third black stone: black stays on two stones and
# play continues.
out=$(run_uci "$ENGINE" "$tmp_ini" swapconn <<'UCI'
setoption name Verbosity value 2
position fen 4k3/8/8/8/8/8/1b6/bA2K3 w - - 0 1 moves b1b2s
go depth 1
UCI
)
assert_continues "$out"

# White captures on a2, leaving black with king and pawn adjacent (the whole
# group connected): black wins even though the capture removed material.
out=$(run_uci "$ENGINE" "$tmp_ini" groupcap <<'UCI'
setoption name Verbosity value 2
position fen 8/8/8/8/8/1Qk5/p1p5/4K3 w - - 0 1 moves b3a2
go depth 1
UCI
)
assert_decisive_end "$out"

# Same capture leaving two disconnected survivors: play continues.
out=$(run_uci "$ENGINE" "$tmp_ini" groupcap <<'UCI'
setoption name Verbosity value 2
position fen 8/8/8/8/4p3/1Qk5/p7/4K3 w - - 0 1 moves b3a2
go depth 1
UCI
)
assert_continues "$out"

echo "connection both-sides regression tests passed"
