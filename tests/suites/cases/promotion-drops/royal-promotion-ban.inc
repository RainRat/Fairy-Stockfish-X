#!/usr/bin/env bash
# A promotion must not field two actual royals outside extinction or
# pseudo-royal coverage (mirrors the startFen single-royal policy). Reaching
# two on-board royals otherwise disables royal resolution: kings stop moving
# entirely while other pieces lose king safety. Extinction-covered variants
# (e.g. checkers) keep promoting to their royal types.

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/../../../../tests/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "royal promotion guard"

load_inline_variants <<'INI'
[rrtest:chess]
promotionPieceTypes = qrk
INI
tmp_ini="${FSX_TMP_INI}"

echo "royal promotion guard tests started"

# White already has a king: promoting a second one is illegal, other
# promotions remain.
out=$(run_uci "$ENGINE" "$tmp_ini" rrtest <<'UCI'
position fen 4k3/1P6/8/8/8/8/8/4K3 w - - 0 1
go perft 1
UCI
)
assert_not_contains "$out" "^b7b8k: 1$" "must not promote a second royal"
assert_contains "$out" "^b7b8q: 1$" "ordinary promotions remain"
assert_nodes "$out" 7

# Shogi-style same-square promotion to a royal is covered by the same gate.
# (Covered implicitly: promotion_allowed backs every promotion path.)

# Extinction-covered checkers still crowns men to kings.
out=$(run_uci "$ENGINE" "$VARIANTS" checkers <<'UCI'
position fen 4k3/1M6/8/8/8/8/8/4K3 w - - 0 1
go perft 1
UCI
)
assert_contains "$out" "^b7a8k: 1$" "extinction-covered royal promotion remains"
assert_contains "$out" "^b7c8k: 1$" "extinction-covered royal promotion remains"
assert_nodes "$out" 4

echo "royal promotion guard tests passed"
