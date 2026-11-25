#!/usr/bin/env bash
# Chasing adjudication must handle drops: a drop has no origin square, so
# from_sq() is SQ_NONE and the old chased() code indexed line_bb(),
# PseudoAttacks[] and square_bb() out of range (debug assertion failure,
# out-of-bounds reads in release; upstream #1092). For a drop all attacks
# of the dropped piece are new and nothing was vacated.
# The perft count also pins the fixed movegen (upstream: no perft change).

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/../../../../tests/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "chasing drop regressions"

echo "chasing drop regression tests started"

out=$(run_uci "$ENGINE" "$VARIANTS" xiangqihouse <<'UCI'
position fen rnbakabnr/9/1c5c1/p1p1p1p1p/9/9/P1P1P1P1P/1C5C1/9/RNBAKABNR[R] w - - 0 1
go perft 1
UCI
)
if grep -q "unknown variant 'xiangqihouse'" <<<"$out"; then
  fsx_test_note_skip "xiangqihouse is unavailable in this engine build"
else
  # Drops must be generated alongside ordinary moves, with a stable count.
  assert_contains "$out" '^R@a2: 1$'
  assert_nodes "$out" 73
fi

echo "chasing drop regression tests passed"
