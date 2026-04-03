#!/bin/bash

set -euo pipefail

error() {
  echo "gating regressions test failed on line $1" >&2
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
ENGINE="${1:-${REPO_ROOT}/src/stockfish}"

run_cmds() {
  local ini=$1
  local cmds=$2
  cat <<EOF | "${ENGINE}" 2>/dev/null
uci
setoption name VariantPath value ${ini}
${cmds}
quit
EOF
}

TMP_INI=$(mktemp)
trap 'rm -f "${TMP_INI}"' EXIT

cat > "${TMP_INI}" <<'EOF'
[gatingblock:chess]
gating = true
seirawanGating = true

[symgating:chess]
gating = true
seirawanGating = true
symmetricDropTypes = r
EOF

# Legal gating move: the gated knight on e1 should block the rook line to h1.
out=$(run_cmds "${TMP_INI}" "setoption name UCI_Variant value gatingblock
position fen 8/8/8/8/8/8/8/R3K2k[N] w KQBCDEFGH - 0 1 moves e1e2n
d")
echo "${out}" | grep -q "Fen: 8/8/8/8/8/8/4K3/R3N2k\\[\\] b - - 1 1"
echo "${out}" | grep -q "^Checkers: *$"

# Symmetric gating must not generate a move that drops onto the mover's destination.
out=$(run_cmds "${TMP_INI}" "setoption name UCI_Variant value symgating
position fen 4k3/8/8/8/8/8/8/4K3[RR] w ABCDEFGH - 0 1
go perft 1")
echo "${out}" | grep -q "^e1d1: 1$"
! echo "${out}" | grep -q "^e1d1r,d1: 1$"
echo "${out}" | grep -q "^e1e2r,d1: 1$"
echo "${out}" | grep -q "^Nodes searched: 9$"

# A legal symmetric gating move keeps the king and adds both gated rooks.
out=$(run_cmds "${TMP_INI}" "setoption name UCI_Variant value symgating
position fen 4k3/8/8/8/8/8/8/4K3[RR] w ABCDEFGH - 0 1 moves e1e2r,d1
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/8/8/8/4K3/3RR3\\[\\] b - - 1 1"