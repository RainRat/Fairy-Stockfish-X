#!/usr/bin/env bash
# A promotion that creates a second pseudo-royal can resolve a check: with
# two royals the checked king loses its sole royalty, so the checking move
# is legal even though the non-promoting version stays in check
# (upstream #1094). Covers shogi-style PIECE_PROMOTION moves (the reported
# hole) as well as the duple-check half.

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/../../../../tests/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "pseudoroyal promotion resolution"

load_inline_variants <<'INI'
[royalpiecepromotion:chess]
king = -
commoner = k
castling = false
extinctionValue = loss
extinctionPieceTypes = k
extinctionPseudoRoyal = true
promotionPieceTypes = -
promotedPieceType = n:k

[royalpiecepromotionduple:royalpiecepromotion]
dupleCheck = true
INI
tmp_ini="${FSX_TMP_INI}"

echo "pseudoroyal promotion resolution regression tests started"

# Sho-shogi drunk elephant promotes to crown prince (uppercase + suffix).
out=$(run_uci "$ENGINE" "$VARIANTS" shoshogi <<'UCI'
position fen 8k/9/9/9/9/4e4/8R/9/4K4 b - - 0 1
go perft 1
UCI
)
assert_contains "$out" '^e4e3\+: 1$'
assert_not_contains "$out" '^e4e3: 1$' "non-promoting elephant move must stay illegal"
assert_nodes "$out" 5

# Synthetic chess-template case from the upstream report.
out=$(run_uci "$ENGINE" "$tmp_ini" royalpiecepromotion <<'UCI'
position fen k7/8/8/8/8/R1n4K/8/8 b - - 0 1
go perft 1
UCI
)
assert_contains "$out" '^c3b1\+: 1$'
assert_contains "$out" '^c3d1\+: 1$'
assert_not_contains "$out" '^c3e2: 1$' "non-promoting knight move must stay illegal"
assert_nodes "$out" 5

# Unless both pseudo-royal pieces are attacked with duple check.
out=$(run_uci "$ENGINE" "$tmp_ini" royalpiecepromotion <<'UCI'
position fen k7/8/8/8/8/2n4K/8/R7 b - - 0 1
go perft 1
UCI
)
assert_contains "$out" '^c3b1\+: 1$'
assert_nodes "$out" 6

out=$(run_uci "$ENGINE" "$tmp_ini" royalpiecepromotionduple <<'UCI'
position fen k7/8/8/8/8/2n4K/8/R7 b - - 0 1
go perft 1
UCI
)
assert_not_contains "$out" '^c3b1\+: 1$' "duple-checked promotion must stay illegal"
assert_nodes "$out" 4

echo "pseudoroyal promotion resolution regression tests passed"
