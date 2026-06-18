#!/bin/bash
# setup-chess regression tests (points-paid drops should not be limited to one of each type)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/lib/uci.sh"

error() {
  echo "setup-chess testing failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=$(default_engine "${1:-}")
VARIANTS=$(default_variants "${2:-}")

echo "setup-chess testing started"

# 1) White can drop a second queen later in setup, as long as points remain.
output=$(run_uci "$ENGINE" "$VARIANTS" setup-chess <<'CMDS'
position startpos moves Q@a1 Q@a8 Q@b1
d
CMDS
)

assert_contains_literal "$output" "Fen: q7/8/8/8/8/8/8/QQ6"

# 2) Points are still enforced (after four white queen drops, another is illegal).
output=$(run_uci "$ENGINE" "$VARIANTS" setup-chess <<'CMDS'
position startpos moves Q@a1 Q@a8 Q@b1 Q@b8 Q@c1 Q@c8 Q@d1 Q@d8
go depth 1
CMDS
)

# White has 3 points left after four queens, so no more queen drops should be considered.
assert_not_contains "$output" " pv .*Q@"

# 3) passUntilSetup: if a side has no affordable setup drop left while the opponent
# still can set up, only pass is legal.
output=$(run_uci "$ENGINE" "$VARIANTS" setup-chess <<'CMDS'
position fen 8/8/8/8/8/8/8/4K3[QQQQRRRRRRRBBBBBBBBBBBBBNNNNNNNNNNNNNPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPkqqqqrrrrrrrbbbbbbbbbbbbbnnnnnnnnnnnnnppppppppppppppppppppppppppppppppppppppp] w - - 0 1 {0 39}
go depth 1
CMDS
)

assert_contains_literal "$output" "bestmove 0000"

# 4) A normal UCI root with no legal moves must stay on the terminal path and
# report MOVE_NONE instead of entering search with an empty root move list.
output=$(run_uci "$ENGINE" "$VARIANTS" chess <<'CMDS'
position fen 7k/5Q2/7K/8/8/8/8/8 b - - 0 1
go depth 1
CMDS
)

assert_contains_literal "$output" "info depth 0 score"
assert_contains_literal "$output" "bestmove (none)"

# 5) Captures in the regular play phase must not refund setup points and unlock drops.
output=$(run_uci "$ENGINE" "$VARIANTS" setup-chess <<'CMDS'
position fen 4k3/8/8/8/8/8/4p3/4K3[P] w - - 0 1 {0 0} moves e1e2 e8e7
go depth 1
CMDS
)

assert_not_contains "$output" " pv .*P@"

echo "setup-chess testing OK"
