#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "nd-tictactoe test"
VARIANT_PATH=${VARIANTS}

echo "nd-tictactoe test started"

if ! variant_available "$ENGINE" tictactoe-3d "$VARIANT_PATH" || \
   ! variant_available "$ENGINE" tictactoe-4d "$VARIANT_PATH"; then
  echo "nd-tictactoe variants not available in this build; skipping nd-tictactoe regression"
  exit 0
fi

# 3D: in-layer row should still win.
out=$(run_uci "$ENGINE" "$VARIANT_PATH" tictactoe-3d <<'EOF'
position fen 3/3/3/3/3/3/3/3/P1P[PPPPPPPPPPppppppppppppp] w - - 0 1 moves P@b1
go perft 1
EOF
)
assert_contains "$out" "^Nodes searched: 0$"

# 3D: a1-b5-c9 is a true 3D line after flattening and must also win.
out=$(run_uci "$ENGINE" "$VARIANT_PATH" tictactoe-3d <<'EOF'
position fen 3/3/3/3/1P1/3/3/3/P2[PPPPPPPPPPppppppppppppp] w - - 0 1 moves P@c9
go perft 1
EOF
)
assert_contains "$out" "^Nodes searched: 0$"

# 4D: in-cell row on the flattened 9x9 board should win.
out=$(run_uci "$ENGINE" "$VARIANT_PATH" tictactoe-4d <<'EOF'
position fen 9/9/9/9/9/9/9/9/P1P6[PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPpppppppppppppppppppppppppppppppppppppppp] w - - 0 1 moves P@b1
go perft 1
EOF
)
assert_contains "$out" "^Nodes searched: 0$"

# 4D: a1-b5-c9 is not enough; a1-b5-c9 represents a different 4D line family only on 3D.
# Use a1-b5-c9's 4D analogue a1-b4-c7? No, test a1-b5-c9 equivalent for 4D flattening: a1,b5,c9 is valid too.
out=$(run_uci "$ENGINE" "$VARIANT_PATH" tictactoe-4d <<'EOF'
position fen 9/9/9/9/1P7/9/9/9/P8[PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPpppppppppppppppppppppppppppppppppppppppp] w - - 0 1 moves P@c9
go perft 1
EOF
)
assert_contains "$out" "^Nodes searched: 0$"

echo "nd-tictactoe test OK"
