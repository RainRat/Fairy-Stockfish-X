#!/usr/bin/env bash
# Regression tests for gating check detection and symmetric gating

set -euo pipefail

error() {
  echo "gating-check-regression testing failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./src/stockfish}

run_cmds() {
  local ini=$1
  local cmds=$2
  printf "uci\nsetoption name VariantPath value ${ini}\n${cmds}\nquit\n" | "${ENGINE}"
}

echo "Running gating-check-regression tests..."

TMP_INI=$(mktemp)
trap 'rm -f "${TMP_INI}"' EXIT

# --- TEST 1: Gated piece blocking discovered check ---
cat > "${TMP_INI}" <<'EOF'
[gatingblock:seirawan]
EOF

# White Rook a1, White King e1. Enemy Black King h1.
# Move King e1-e2 and gate a Pawn (p) on e1.
# The gated Pawn at e1 MUST block the discovered check from Rook a1 to King h1.
OUT=$(run_cmds "${TMP_INI}" "setoption name UCI_Variant value gatingblock
position fen 8/8/8/8/8/8/8/R3K2k[P] w KQ - 0 1 moves e1e2p
d")

if echo "$OUT" | grep -q "Checkers: [a-h][1-8]"; then
    echo "FAILED: Engine thinks it is check, but it should be blocked by the gated piece!"
    exit 1
fi

# --- TEST 2: Castling gating occupancy (prevent false positive check) ---
# White King e1, Rook h1. Enemy Black King a1.
# Gate a Rook (R) on h1 while castling kingside (e1g1).
# Gated Rook at h1 would give check to a1 IF g1 and f1 were empty.
# But King at g1 and Rook at f1 must block this check.
OUT=$(run_cmds "${TMP_INI}" "setoption name UCI_Variant value gatingblock
position fen 8/8/8/8/8/8/8/k3K2R[R] w KQBCDEFGH - 0 1 moves e1g1rh1
d")

if echo "$OUT" | grep -q "Checkers: [a-h][1-8]"; then
    echo "FAILED: Engine thinks it is check, but it should be blocked by castled pieces!"
    exit 1
fi

# --- TEST 3: Symmetric Gating Generation ---
cat > "${TMP_INI}" <<'EOF'
[symgating:chess]
gating = true
seirawanGating = true
symmetricDropTypes = r
EOF

# White King e1, pocket has [RR].
# Standard moves: e1d1, e1f1, e1d2, e1e2, e1f2 (5 moves)
# Gating moves: e1d1r,d1; e1f1r,d1; e1d2r,d1; e1e2r,d1; e1f2r,d1 (5 moves)
# Total: 10 moves.
OUT=$(run_cmds "${TMP_INI}" "setoption name UCI_Variant value symgating
position fen 4k3/8/8/8/8/8/8/4K3[RR] w ABCDEFGH - 0 1
go perft 1")

NODES=$(echo "$OUT" | grep "Nodes searched:" | awk '{print $3}')
if [ "$NODES" -ne 10 ]; then
    echo "FAILED: Expected 10 nodes for symmetric gating, found $NODES"
    exit 1
fi

echo "gating-check-regression testing OK"
