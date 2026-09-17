#!/usr/bin/env bash
# Pin the simultaneous terminal-condition precedence documented in
# src/variants.ini ("Simultaneous terminal conditions and precedence").
# Each tie below makes two goal-like rules true at once with doctored values
# chosen so the winner is observable (mate vs draw kind); the paired control
# proves the later rule fires on its own, so the tie result really pins the
# order rather than a dead rule.

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/../../../../tests/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "simultaneous terminal precedence"

load_inline_variants <<'INI'
[simul-extinct-connect:chess]
extinctionValue = loss
extinctionPieceTypes = r
connectN = 3
connectPieceTypes = p
connectHorizontal = true
connectVertical = false
connectDiagonal = false
connectValue = draw

[simul-points-connect:chess]
pointsCounting = true
pointsGoal = 3
connectN = 3
connectPieceTypes = p
connectHorizontal = true
connectVertical = false
connectDiagonal = false
connectValue = draw

[simul-flag-extinct:chess]
extinctionValue = draw
extinctionPieceTypes = r
flagPieceWhite = q
flagRegionWhite = h8

[simul-threshold-extinct:chess]
extinctionValue = loss
extinctionPieceTypesWhite = q
nonRoyalDrawThreshold = 2

[simul-threshold-only:chess]
nonRoyalDrawThreshold = 2
INI
tmp_ini="${FSX_TMP_INI}"

assert_decisive_end() {
  local out="$1"
  assert_contains "$out" "info string adjudication reason game_end"
  assert_contains "$out" "result mate"
  assert_contains "$out" "^bestmove \(none\)$"
}

assert_drawn_end() {
  local out="$1"
  assert_contains "$out" "info string adjudication reason game_end"
  assert_contains "$out" "result cp 0"
  assert_contains "$out" "^bestmove \(none\)$"
}

echo "simultaneous terminal precedence tests started"

# Extinction beats connection: white is connected and rookless, so the
# connection draw loses to the extinction mate for black to move ...
out=$(run_uci "$ENGINE" "$tmp_ini" simul-extinct-connect <<'UCI'
setoption name Verbosity value 2
position fen 4k2r/8/8/8/8/PPP5/8/4K3 b - - 0 1
go depth 1
UCI
)
assert_decisive_end "$out"

# ... while the connection draw fires on its own when white keeps a rook.
out=$(run_uci "$ENGINE" "$tmp_ini" simul-extinct-connect <<'UCI'
setoption name Verbosity value 2
position fen r3k3/8/8/8/8/PPP5/8/4K2R b - - 0 1
go depth 1
UCI
)
assert_drawn_end "$out"

# Points beat connection: white is over goal and black is connected, so the
# points mate for black to move wins over the connection draw ...
out=$(run_uci "$ENGINE" "$tmp_ini" simul-points-connect <<'UCI'
setoption name Verbosity value 2
position fen 4k3/8/8/8/8/ppp5/8/4K2Q b - - 0 1 {5 0}
go depth 1
UCI
)
assert_decisive_end "$out"

# ... while the connection draw fires on its own below the points goal.
out=$(run_uci "$ENGINE" "$tmp_ini" simul-points-connect <<'UCI'
setoption name Verbosity value 2
position fen 4k3/8/8/8/8/ppp5/8/4K2Q b - - 0 1 {0 0}
go depth 1
UCI
)
assert_drawn_end "$out"

# Extinction beats flag: white is flagged (Qh8) and rookless, so the
# extinction draw wins over the flag mate against black to move ...
out=$(run_uci "$ENGINE" "$tmp_ini" simul-flag-extinct <<'UCI'
setoption name Verbosity value 2
position fen 4k2Q/8/8/8/8/8/8/R3K3 b - - 0 1
go depth 1
UCI
)
assert_drawn_end "$out"

# ... while the flag mate fires on its own when white keeps a rook.
out=$(run_uci "$ENGINE" "$tmp_ini" simul-flag-extinct <<'UCI'
setoption name Verbosity value 2
position fen 1r2k2Q/8/8/8/8/8/8/R3K3 b - - 0 1
go depth 1
UCI
)
assert_decisive_end "$out"

# Extinction beats the non-royal threshold: 1-vs-1 counts meet the threshold
# but white is queenless, so the extinction mate wins over the threshold
# draw ...
out=$(run_uci "$ENGINE" "$tmp_ini" simul-threshold-extinct <<'UCI'
setoption name Verbosity value 2
position fen 3qk3/8/8/8/8/8/8/R3K3 b - - 0 1
go depth 1
UCI
)
assert_decisive_end "$out"

# ... while the threshold draw fires on its own without extinction.
out=$(run_uci "$ENGINE" "$tmp_ini" simul-threshold-only <<'UCI'
setoption name Verbosity value 2
position fen 3qk3/8/8/8/8/8/8/R3K3 b - - 0 1
go depth 1
UCI
)
assert_drawn_end "$out"

echo "simultaneous terminal precedence tests passed"
