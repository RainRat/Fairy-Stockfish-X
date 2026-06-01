#!/bin/bash
# Pond regression tests (line removal, terminal adjudication, search stability)

set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

ENGINE=$(default_engine "${1:-}")
VARIANTS=$(default_variants "${2:-}")

error() {
  echo "pond testing failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

echo "pond testing started"

# 1) Crash regression for deep-ish search from known failing position
output=$(run_uci "$ENGINE" "$VARIANTS" pond <<CMDS
position fen 1tt1/T1et/2TF/T1ef[EEEEEEeeeee] w - - 0 11 {3 2}
go movetime 1000
CMDS
)

assert_contains "$output" "^bestmove "
assert_not_contains "$output" "(Assertion|Segmentation fault|Aborted|Illegal instruction)"

# 2) Original 3-in-a-row removal regression: all tadpoles removed as expected
output=$(run_uci "$ENGINE" "$VARIANTS" pond <<CMDS
position startpos moves E@b3 E@d3 E@c3 E@c2
d
CMDS
)

assert_contains_literal "$output" "Fen: 4/4/2e1/4[EEEEEEEEEEEeeeeeeeeeee] w - - 0 3 {2 1}"

# 3) Tadpoles move to empty adjacent orthogonal squares but do not capture by replacement
output=$(run_uci "$ENGINE" "$VARIANTS" pond <<CMDS
position fen 4/4/1Tt1/4[] w - - 0 1 {0 0}
go perft 1
CMDS
)

assert_contains_literal "$output" "b2b1: 1"
assert_contains_literal "$output" "b2a2: 1"
assert_contains_literal "$output" "b2b3: 1"
assert_not_contains_literal "$output" "b2c2:"

# 4) Frogs can move one or two squares orthogonally, but never capture by replacement
output=$(run_uci "$ENGINE" "$VARIANTS" pond <<CMDS
position fen 4/4/Ft2/4[] w - - 0 1 {0 0}
go perft 1
CMDS
)

assert_contains_literal "$output" "a2a1: 1"
assert_contains_literal "$output" "a2a3: 1"
assert_contains_literal "$output" "a2c2: 1"

output=$(run_uci "$ENGINE" "$VARIANTS" pond <<CMDS
position fen 4/4/1F2/4[] w - - 0 1 {0 0}
go perft 1
CMDS
)

assert_contains_literal "$output" "b2d2: 1"

output=$(run_uci "$ENGINE" "$VARIANTS" pond <<CMDS
position fen 4/4/1Ft1/4[] w - - 0 1 {0 0}
go perft 1
CMDS
)

assert_contains_literal "$output" "b2d2: 1"

output=$(run_uci "$ENGINE" "$VARIANTS" pond <<CMDS
position fen 4/4/1FtT/4[] w - - 0 1 {0 0}
go perft 1
CMDS
)

assert_not_contains_literal "$output" "b2d2:"

# 5) Simultaneous removeConnectN: two horizontal lines removed in one move
output=$(run_uci "$ENGINE" "$VARIANTS" pond <<CMDS
position fen TTT1/4/3T/TTT1 w - - 0 1 {0 0} moves d2d3
d
CMDS
)

assert_contains_literal "$output" "Fen: 4/3T/4/4[] b - - 1 1 {6 0}"

# 6) Corner-edge diagonal removeConnectN
output=$(run_uci "$ENGINE" "$VARIANTS" pond <<CMDS
position fen T3/2T1/1T2/T3 w - - 0 1 {0 0} moves a4b4
d
CMDS
)

assert_contains_literal "$output" "Fen: 1T2/4/4/4[] b - - 1 1 {3 0}"

# 7) Stalemate terminal check (no legal moves means loss in pond)
output=$(run_uci "$ENGINE" "$VARIANTS" pond <<CMDS
position fen 1T2/4/4/4[] b - - 1 1 {3 0}
go depth 2
CMDS
)

assert_contains_literal "$output" "bestmove (none)"

echo "pond testing OK"
