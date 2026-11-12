#!/usr/bin/env bash
# Racing kings stay royal: checking=false forbids giving check but does not
# make kings capturable (see variants.ini checking docs). Only allowChecks
# governs capturability (pinned the other way by no-royal-capture.sh).
# Each fixture loads a position with the enemy king under attack; the
# king capture must be absent while ordinary moves remain.

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/../../../../tests/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "racing royal capture"

echo "racing royal capture regression tests started"

# racingchess: queen takes king was legal before the guard fix (19 nodes).
out=$(run_uci "$ENGINE" "$VARIANTS" racingchess <<'UCI'
position fen 4k3/8/8/8/8/8/4Q3/4K3 w - - 0 1
go perft 1
UCI
)
assert_not_contains "$out" "^e2e8: 1$" "racingchess must not capture the king"
assert_nodes "$out" 18

# checkmateless: same shape, same expectation.
out=$(run_uci "$ENGINE" "$VARIANTS" checkmateless <<'UCI'
position fen 4k3/8/8/8/8/8/4Q3/4K3 w - - 0 1
go perft 1
UCI
)
assert_not_contains "$out" "^e2e8: 1$" "checkmateless must not capture the king"
assert_nodes "$out" 18

# royal-race needs a 9x7 board; skip on engines without large-board coverage.
out=$(run_uci "$ENGINE" "$VARIANTS" royal-race <<'UCI'
position fen 3k3/5Y1/7/7/7/7/7/7/3K3 w - - 0 1
go perft 1
UCI
)
if grep -q "unknown variant 'royal-race'" <<<"$out"; then
  fsx_test_note_skip "royal-race is unavailable in this engine build"
else
  assert_not_contains "$out" "^f8d9: 1$" "royal-race must not capture the king"
  assert_nodes "$out" 6
fi

# Controls: orthodox chess and mate-only checkless never allowed the capture.
out=$(run_uci "$ENGINE" "$VARIANTS" chess <<'UCI'
position fen 4k3/8/8/8/8/8/4Q3/4K3 w - - 0 1
go perft 1
UCI
)
assert_not_contains "$out" "^e2e8: 1$" "chess must not capture the king"
assert_nodes "$out" 25

out=$(run_uci "$ENGINE" "$VARIANTS" checkless <<'UCI'
position fen 4k3/8/8/8/8/8/4Q3/4K3 w - - 0 1
go perft 1
UCI
)
assert_not_contains "$out" "^e2e8: 1$" "checkless must not capture the king"
assert_nodes "$out" 18

echo "racing royal capture regression tests passed"
