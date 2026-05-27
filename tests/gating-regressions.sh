#!/bin/bash

set -euo pipefail

error() {
  echo "gating regressions test failed on line $1" >&2
  exit 1
}
trap 'error ${LINENO}' ERR

source "$(dirname "${BASH_SOURCE[0]}")/lib/uci.sh"

ENGINE=$(default_engine "${1:-}")

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
out=$(run_uci "$ENGINE" "${TMP_INI}" "gatingblock" <<<'position fen 8/8/8/8/8/8/8/R3K2k[N] w KQBCDEFGH - 0 1 moves e1e2n
d')
assert_contains "$out" "Fen: 8/8/8/8/8/8/4K3/R3N2k\\[\\] b - - 1 1"
assert_contains "$out" "^Checkers: *$"

# Symmetric gating must not generate a move that drops onto the mover's destination.
out=$(run_uci "$ENGINE" "${TMP_INI}" "symgating" <<<'position fen 4k3/8/8/8/8/8/8/4K3[RR] w ABCDEFGH - 0 1
go perft 1')
assert_contains "$out" "^e1d1: 1$"
assert_not_contains "$out" "^e1d1r,d1: 1$"
assert_contains "$out" "^e1e2r,d1: 1$"
assert_nodes "$out" 9

# A legal symmetric gating move keeps the king and adds both gated rooks.
out=$(run_uci "$ENGINE" "${TMP_INI}" "symgating" <<<'position fen 4k3/8/8/8/8/8/8/4K3[RR] w ABCDEFGH - 0 1 moves e1e2r,d1
d')
assert_contains "$out" "Fen: 4k3/8/8/8/8/8/4K3/3RR3\\[\\] b - - 1 1"
