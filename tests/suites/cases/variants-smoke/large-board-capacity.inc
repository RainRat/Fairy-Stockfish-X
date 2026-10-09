#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR=$(cd "${SCRIPT_DIR}/../../../.." && pwd)
source "${SCRIPT_DIR}/../../../../tests/lib/uci.sh"

# Prefer the named auxiliary role, while allowing focused DEBUG_STATEINFO runs
# to select a special VLB engine without changing the normal suite engine.
ENGINE="${VLB_ENGINE:-${ROOT_DIR}/src/stockfish-vlb}"
VARIANTS="${2:-${ROOT_DIR}/src/variants.ini}"
setup_test_context "${ENGINE}" "${VARIANTS}" "large-board position capacity"

# DEBUG_STATEINFO checks must use the configured board capacity. Both positions
# are legal on a 16x16 board and exceed 64 pieces for White; the first also has
# more than 64 pawns. They cover the distinct pawn-count and occupancy checks.
load_inline_variants <<'INI'
[vlb-over64-pawns:fairy]
maxFile = p
maxRank = 16
startFen = 15k/16/16/16/16/16/16/16/16/16/16/PP14/PPPPPPPPPPPPPPPP/PPPPPPPPPPPPPPPP/PPPPPPPPPPPPPPPP/KPPPPPPPPPPPPPPP w - - 0 1

[vlb-over64-pieces:fairy]
maxFile = p
maxRank = 16
startFen = 15k/16/16/16/16/16/16/16/16/16/16/NN14/NNNNNNNNNNNNNNNN/NNNNNNNNNNNNNNNN/NNNNNNNNNNNNNNNN/KNNNNNNNNNNNNNNN w - - 0 1
INI

for variant in vlb-over64-pawns vlb-over64-pieces; do
  output=$(run_uci "${ENGINE}" "${FSX_TMP_INI}" "${variant}" <<'EOF' 2>&1
position startpos
d
EOF
)
  assert_contains_literal "${output}" "Fen:"
done

# Chu Shogi permits checks, so a black King attacked by White's rook is a
# valid position. Searching its bare-king material-depletion draw can reach
# the same allowed attack state in a null-move probe; DEBUG_STATEINFO must not
# apply the orthodox side-not-to-move safety invariant there.
output=$(run_uci "${ENGINE}" "${VARIANTS}" chu_shogi <<'EOF' 2>&1
position fen 10Rk/12/12/12/12/12/12/12/12/12/12/K11 b - - 0 1
go depth 1
EOF
)
assert_contains_literal "${output}" "score cp 0"

echo "large-board capacity regressions passed"
