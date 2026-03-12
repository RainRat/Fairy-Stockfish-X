#!/bin/bash

set -euo pipefail

ENGINE=${1:-src/stockfish}
VARIANT_PATH=${2:-src/variants.ini}

run_cmds() {
  cat <<EOF | "${ENGINE}"
uci
setoption name VariantPath value ${VARIANT_PATH}
$1
quit
EOF
}

echo "nd-tictactoe test started"

# 3D: in-layer row should still win.
out=$(run_cmds "setoption name UCI_Variant value tictactoe-3d
position fen 3/3/3/3/3/3/3/3/P1P[PPPPPPPPPPppppppppppppp] w - - 0 1 moves P@b1
go perft 1")
echo "${out}" | grep -q "Nodes searched: 0"

# 3D: a1-b5-c9 is a true 3D line after flattening and must also win.
out=$(run_cmds "setoption name UCI_Variant value tictactoe-3d
position fen 3/3/3/3/1P1/3/3/3/P2[PPPPPPPPPPppppppppppppp] w - - 0 1 moves P@c9
go perft 1")
echo "${out}" | grep -q "Nodes searched: 0"

# 4D: in-cell row on the flattened 9x9 board should win.
out=$(run_cmds "setoption name UCI_Variant value tictactoe-4d
position fen 9/9/9/9/9/9/9/9/P1P6[PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPpppppppppppppppppppppppppppppppppppppppp] w - - 0 1 moves P@b1
go perft 1")
echo "${out}" | grep -q "Nodes searched: 0"

# 4D: a1-b5-c9 is not enough; a1-b5-c9 represents a different 4D line family only on 3D.
# Use a1-b5-c9's 4D analogue a1-b4-c7? No, test a1-b5-c9 equivalent for 4D flattening: a1,b5,c9 is valid too.
out=$(run_cmds "setoption name UCI_Variant value tictactoe-4d
position fen 9/9/9/9/1P7/9/9/9/P8[PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPpppppppppppppppppppppppppppppppppppppppp] w - - 0 1 moves P@c9
go perft 1")
echo "${out}" | grep -q "Nodes searched: 0"

echo "nd-tictactoe test OK"
