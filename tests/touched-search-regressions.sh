#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "touched search regressions"

echo "touched search regressions started"

# Hash resizing to the supported minimum should keep the engine alive and ready.
out=$(run_engine_stdin "$ENGINE" $'uci\nsetoption name Hash value 1\nisready\nquit\n')
assert_contains_literal "$out" "readyok"

# Terminal UCI roots should stay on the root-terminal path and produce MOVE_NONE.
out=$(run_uci "$ENGINE" "$VARIANTS" chess <<'EOF'
position fen 7k/5Q2/7K/8/8/8/8/8 b - - 0 1
go depth 1
EOF
)
assert_contains_literal "$out" "info depth 0 score"
assert_contains_literal "$out" "bestmove (none)"

# Empty-destination captures such as en passant must remain usable by search
# and history indexing after captured-type deduplication.
out=$(run_uci "$ENGINE" "$VARIANTS" chess <<'EOF'
position fen 4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1
go perft 1
go depth 3
EOF
)
assert_contains "$out" "^e5d6: 1$"
assert_contains "$out" "^bestmove "

# Spell-chess jump potions exercise deferred potion generation and pruning
# without linking a board-size-specific C++ harness.
if variant_available "$ENGINE" spell-chess "$VARIANTS"; then
  out=$(run_uci "$ENGINE" "$VARIANTS" spell-chess <<'EOF'
position fen 7k/8/8/p7/8/p7/8/R3K3[J] w - - 0 1
go perft 1
EOF
)
  assert_contains "$out" "^j@a3,a1a4: 1$"
  assert_contains "$out" "^j@a3,a1a5: 1$"

  out=$(run_uci "$ENGINE" "$VARIANTS" spell-chess <<'EOF'
position fen k6r/8/8/8/8/8/8/R1p4K[J] w - - 0 1
go perft 1
EOF
)
  assert_contains "$out" "^j@c1,a1g1: 1$"
fi

# Mini Hexchess previously produced an immediate false mate score after this
# first move. Run only when the current engine/variant set supports it.
if variant_available "$ENGINE" minihexchess "$VARIANTS"; then
  out=$(run_uci "$ENGINE" "$VARIANTS" minihexchess <<'EOF'
position startpos moves c3d4 g6d5
go perft 1
EOF
)
  assert_contains "$out" "^b2c3: 1$"

  out=$(run_uci "$ENGINE" "$VARIANTS" minihexchess <<'EOF'
position startpos moves c2d3
go movetime 100
go movetime 400
EOF
)
  assert_contains "$out" "^bestmove [a-g][1-7][a-g][1-7]"
  assert_contains "$out" "score cp"
  assert_not_contains "$out" "score mate"
fi

echo "touched search regressions passed"
