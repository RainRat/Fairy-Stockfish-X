#!/bin/bash
# setup-chess regression tests (points-paid drops should not be limited to one of each type)

set -euo pipefail

cd "$(dirname "$0")/../src"

error() {
  echo "setup-chess testing failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./stockfish}

echo "setup-chess testing started"

run_uci() {
  local cmd_file
  cmd_file=$(mktemp)
  cat > "$cmd_file"
  local out
  out=$(mktemp)
  timeout 20s "$ENGINE" < "$cmd_file" > "$out" 2>&1
  rm -f "$cmd_file"
  cat "$out"
  rm -f "$out"
}

# 1) White can drop a second queen later in setup, as long as points remain.
output=$(run_uci <<'CMDS'
uci
setoption name VariantPath value variants.ini
setoption name UCI_Variant value setup-chess
position startpos moves Q@a1 Q@a8 Q@b1
d
quit
CMDS
)

echo "$output" | grep -Fq "Fen: q7/8/8/8/8/8/8/QQ6"

# 2) Points are still enforced (after four white queen drops, another is illegal).
output=$(run_uci <<'CMDS'
uci
setoption name VariantPath value variants.ini
setoption name UCI_Variant value setup-chess
position startpos moves Q@a1 Q@a8 Q@b1 Q@b8 Q@c1 Q@c8 Q@d1 Q@d8
go depth 1
quit
CMDS
)

# White has 3 points left after four queens, so no more queen drops should be considered.
! echo "$output" | grep -Eq " pv .*Q@"

# 3) passUntilSetup: if a side has no affordable setup drop left while the opponent
# still can set up, only pass is legal.
output=$(run_uci <<'CMDS'
uci
setoption name VariantPath value variants.ini
setoption name UCI_Variant value setup-chess
position fen 8/8/8/8/8/8/8/4K3[QQQQRRRRRRRBBBBBBBBBBBBBNNNNNNNNNNNNNPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPkqqqqrrrrrrrbbbbbbbbbbbbbnnnnnnnnnnnnnppppppppppppppppppppppppppppppppppppppp] w - - 0 1 {0 39}
go depth 1
quit
CMDS
)

echo "$output" | grep -Fq "bestmove e1e1"

# 4) Captures in the regular play phase must not refund setup points and unlock drops.
output=$(run_uci <<'CMDS'
uci
setoption name VariantPath value variants.ini
setoption name UCI_Variant value setup-chess
position fen 4k3/8/8/8/8/8/4p3/4K3[P] w - - 0 1 {0 0} moves e1e2 e8e7
go depth 1
quit
CMDS
)

! echo "$output" | grep -Eq " pv .*P@"

echo "setup-chess testing OK"
