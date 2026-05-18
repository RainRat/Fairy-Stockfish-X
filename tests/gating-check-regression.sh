#!/usr/bin/env bash
# Regression tests for gating check detection and symmetric gating

set -euo pipefail

error() {
  echo "gating-check-regression testing failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./src/stockfish}
ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
CXX=${CXX:-g++}

run_cmds() {
  local ini=$1
  local cmds=$2
  printf "uci\nsetoption name VariantPath value ${ini}\n${cmds}\nquit\n" | "${ENGINE}"
}

echo "Running gating-check-regression tests..."

TMP_INI=$(mktemp)
TMP_CPP=""
TMP_BIN=""
trap 'rm -f "${TMP_INI}" "${TMP_CPP}" "${TMP_BIN}"' EXIT

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
# Gating moves: e1f1r,d1; e1d2r,d1; e1e2r,d1; e1f2r,d1 (4 moves). e1d1r is illegal as King is on d1.
# Total: 9 moves.
OUT=$(run_cmds "${TMP_INI}" "setoption name UCI_Variant value symgating
position fen 4k3/8/8/8/8/8/8/4K3[RR] w ABCDEFGH - 0 1
go perft 1")

NODES=$(echo "$OUT" | grep "Nodes searched:" | awk '{print $3}')
if [ "$NODES" -ne 9 ]; then
    echo "FAILED: Expected 9 nodes for symmetric gating, found $NODES"
    exit 1
fi

# --- TEST 4: Encoded gate/pull squares must preserve the last board square ---
TMP_CPP=$(mktemp /tmp/gating-check-XXXXXX.cpp)
TMP_BIN=$(mktemp /tmp/gating-check-XXXXXX)
cat > "${TMP_CPP}" <<'EOF'
#include <cassert>
#include "types.h"

using namespace Stockfish;

int main() {
    assert(gating_square(make_gating<NORMAL>(SQ_A1, SQ_A2, NO_PIECE_TYPE, SQ_H8)) == SQ_H8);
    assert(gating_square(make_gating<CASTLING>(SQ_E1, SQ_G1, ROOK, SQ_H8)) == SQ_H8);
    assert(pull_square(make_pull(SQ_A1, SQ_A2, SQ_H8)) == SQ_H8);
    assert(is_gating(make_gating<NORMAL>(SQ_A1, SQ_A2, NO_PIECE_TYPE, SQ_H8)));
    return 0;
}
EOF
rm -f "${TMP_BIN}"
"${CXX}" -std=c++17 -O2 -Wall -Wextra -I"${ROOT_DIR}/src" "${TMP_CPP}" -o "${TMP_BIN}"
"${TMP_BIN}"

echo "gating-check-regression testing OK"
