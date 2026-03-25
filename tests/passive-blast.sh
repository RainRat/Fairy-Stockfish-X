#!/bin/bash

set -euo pipefail

error() {
  echo "passive-blast test failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-}
if [[ -z "${ENGINE}" ]]; then
  if [[ -x "src/stockfish" ]]; then
    ENGINE="src/stockfish"
  else
    ENGINE="./stockfish"
  fi
fi

tmp_ini=$(mktemp)
trap 'rm -f "${tmp_ini}"' EXIT

cat > "${tmp_ini}" <<'INI'
[passive-blast-test:chess]
customPiece1 = f:W
blastPassiveTypes = f
pieceToCharTable = ...............F....K...............f....k
INI

run_cmds() {
  cat <<EOF | "${ENGINE}" 2>/dev/null
uci
setoption name VariantPath value ${tmp_ini}
setoption name UCI_Variant value passive-blast-test
$1
quit
EOF
}

# Kings may not move adjacent to an enemy passive blaster.
out=$(run_cmds "position fen 4k3/8/8/4f3/8/4K3/8/8 w - - 0 1
go perft 1")
! echo "${out}" | grep -q "^e3e4: 1$"

# Positions loaded with a king adjacent to an enemy passive blaster are treated as check.
out=$(run_cmds "position fen r3k3/4f3/8/8/8/8/4K3/8 b - - 0 1
go perft 1")
! echo "${out}" | grep -q "^a8a7: 1$"

# Moves that create passive-blast adjacency also set check state for the reply.
out=$(run_cmds "position fen r3k3/8/4f3/8/8/8/4K3/8 w - - 0 1 moves e6e7
go perft 1")
! echo "${out}" | grep -q "^a8a7: 1$"

# Ordinary enemy pieces adjacent to a passive blaster are removed after the move.
out=$(run_cmds "position fen 4k3/8/8/4f3/8/4R3/4K3/8 w - - 0 1 moves e3e4
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/4f3/8/8/4K3/8 b - - 1 1"

# blastImmuneTypes composes with passive blast.
cat > "${tmp_ini}" <<'INI'
[passive-blast-immune:chess]
customPiece1 = f:W
blastPassiveTypes = f
blastImmuneTypes = r
pieceToCharTable = ....R..........F....K....r..........f....k
INI
out=$(run_cmds "setoption name UCI_Variant value passive-blast-immune
position fen 4k3/8/8/4f3/8/4R3/4K3/8 w - - 0 1 moves e3e4
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/4f3/4R3/8/4K3/8 b - - 1 1"
