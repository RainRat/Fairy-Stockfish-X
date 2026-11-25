#!/usr/bin/env bash
# Atomic mate on the 50th move must not be scored as a move-count draw.
# Upstream #1054: the n-move rule read royal-only check state and declared
# a draw over a pseudo-royal mate at rule50 == 2*nMoveRule. FSX evasion state
# tracks royal checks only, so the same report reproduced here (g2g1/cp 0 at
# 99/50 instead of 50. Rf1#); the n-move guard now also consults pseudo-royal
# checks. Uses expect (not a bare pipe) so the search is not aborted by quit.

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/../../../../tests/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "atomic fifty-move mate"

echo "atomic fifty-move mate regression tests started"

# Mate on the 50th move: rule50 == 99 + Rf1 reaches the limit exactly.
out=$(run_expect "$ENGINE" "$VARIANTS" <<EOF
$(expect_engine_setup)
 set variant_path [lindex \$argv 1]
 send "uci\n"
 expect "uciok"
 send "setoption name VariantPath value \$variant_path\n"
 send "setoption name UCI_Variant value atomic\n"
 send "position fen 8/8/8/8/5R2/8/6R1/3Kn2k w - - 99 50\n"
 send "go depth 3\n"
 expect "score mate 1"
 expect "bestmove f4f1"
 send "quit\n"
 expect eof
EOF
)
assert_contains "$out" 'score mate 1'
assert_contains "$out" '^bestmove f4f1'

# Control: one half-move earlier the same mate is found (pre-existing behavior).
out=$(run_expect "$ENGINE" "$VARIANTS" <<EOF
$(expect_engine_setup)
 set variant_path [lindex \$argv 1]
 send "uci\n"
 expect "uciok"
 send "setoption name VariantPath value \$variant_path\n"
 send "setoption name UCI_Variant value atomic\n"
 send "position fen 8/8/8/8/5R2/8/6R1/3Kn2k w - - 98 50\n"
 send "go depth 3\n"
 expect "score mate 1"
 expect "bestmove f4f1"
 send "quit\n"
 expect eof
EOF
)
assert_contains "$out" 'score mate 1'
assert_contains "$out" '^bestmove f4f1'

echo "atomic fifty-move mate regression tests passed"
