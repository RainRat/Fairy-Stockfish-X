#!/usr/bin/env bash
# Regression tests for nonRoyalDrawThreshold: draw when both sides have
# fewer than N non-royal pieces, with delivered mates taking precedence
# (the rule is only assessed when the side to move is not in check).

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/../../../../tests/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "non-royal draw threshold regression"

load_inline_variants <<'INI'
[nonroyal-draw:chess]
nonRoyalDrawThreshold = 2

[nonroyal-draw-bare:chess]
nonRoyalDrawThreshold = 1

[nonroyal-draw-hand:nonroyal-draw]
pieceDrops = true
INI
tmp_ini="${FSX_TMP_INI}"

assert_drawn_end() {
  local out="$1"
  assert_contains "$out" "info string adjudication reason game_end"
  assert_contains "$out" "result cp 0"
  assert_contains "$out" "^bestmove \(none\)$"
}

echo "non-royal draw threshold regression tests started"

# Quiet 1-vs-1 endgame is drawn immediately.
out=$(run_uci "$ENGINE" "$tmp_ini" nonroyal-draw <<'UCI'
setoption name Verbosity value 2
position fen 4k3/8/8/8/8/8/8/R3K3 w - - 0 1
go depth 1
UCI
)
assert_drawn_end "$out"

# 2-vs-1 is not a draw: the game continues.
out=$(run_uci "$ENGINE" "$tmp_ini" nonroyal-draw <<'UCI'
setoption name Verbosity value 2
position fen 4k3/8/8/8/8/8/8/RR2K3 w - - 0 1
go depth 1
UCI
)
assert_not_contains "$out" "adjudication reason game_end"

# Delivered mate wins even though the resulting 1-vs-1 meets the threshold:
# Qxh7 captures black's last-but-one piece with checkmate. Adjudicating the
# mated position yields mate, not a threshold draw (mate takes precedence;
# the rule is skipped while the side to move is in check).
out=$(run_uci "$ENGINE" "$tmp_ini" nonroyal-draw <<'UCI'
setoption name Verbosity value 2
position fen 1r5k/7n/6KQ/8/8/8/8/8 w - - 0 1 moves h6h7
go depth 1
UCI
)
assert_contains "$out" "adjudication reason checkmate"
assert_contains "$out" "result mate"
assert_contains "$out" "^bestmove \(none\)$"

# Threshold met but side to move is in check: the game continues so the
# mating reply stays possible (moves cannot be appended past a terminal
# position, so the check is encoded directly: Qd1+ with Kxd1/Rxd1 escapes).
out=$(run_uci "$ENGINE" "$tmp_ini" nonroyal-draw <<'UCI'
setoption name Verbosity value 2
position fen 2k5/8/8/8/8/8/8/R2qK3 w - - 0 1
go depth 1
UCI
)
assert_not_contains "$out" "adjudication reason game_end"
assert_not_contains "$out" "^bestmove \(none\)$"

# Bare-vs-bare draws under threshold 1 ...
out=$(run_uci "$ENGINE" "$tmp_ini" nonroyal-draw-bare <<'UCI'
setoption name Verbosity value 2
position fen 4k3/8/8/8/8/8/8/4K3 w - - 0 1
go depth 1
UCI
)
assert_drawn_end "$out"

# ... while 1-vs-1 does not (fewer than one means zero here).
out=$(run_uci "$ENGINE" "$tmp_ini" nonroyal-draw-bare <<'UCI'
setoption name Verbosity value 2
position fen 4k3/8/8/8/8/8/4R3/4K3 w - - 0 1
go depth 1
UCI
)
assert_not_contains "$out" "adjudication reason game_end"

# Pieces in hand count: rook on board plus knight in hand is two non-royals.
out=$(run_uci "$ENGINE" "$tmp_ini" nonroyal-draw-hand <<'UCI'
setoption name Verbosity value 2
position fen 4k3/8/8/8/8/8/8/R3K3[N] w - - 0 1
go depth 1
UCI
)
assert_not_contains "$out" "adjudication reason game_end"

echo "non-royal draw threshold regression tests passed"
