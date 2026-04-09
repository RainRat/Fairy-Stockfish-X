#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ENGINE="${1:-${SCRIPT_DIR}/../src/stockfish}"
VARIANTS="${2:-${SCRIPT_DIR}/../src/variants.ini}"

die() {
  echo "crazy-cavalier regression failed on line $1" >&2
  exit 1
}
trap 'die $LINENO' ERR

run_cmds() {
  {
    echo "setoption name VariantPath value ${VARIANTS}"
    echo "setoption name UCI_Variant value crazy-cavalier"
    printf '%s\n' "$1"
    echo quit
  } | "${ENGINE}" 2>&1
}

# Start position loads and exposes the reconstructed 9x10 setup.
out=$(run_cmds "position startpos
go perft 1")
if grep -q "Variant 'crazy-cavalier' exceeds build board limits" <<<"${out}"; then
  echo "skip: crazy-cavalier requires a larger-board capable engine"
  exit 0
fi
echo "${out}" | grep -q "info string variant crazy-cavalier files 9 ranks 10"

# Sergeants can move sideways quietly under the reconstructed Sideways rule.
out=$(run_cmds "position fen 9/9/9/9/9/9/9/4D4/9/5k3 w - - 0 1
go perft 1")
echo "${out}" | grep -q "^e3d3: 1$"
echo "${out}" | grep -q "^e3f3: 1$"

# Sergeants promote to a non-royal king/commoner from the configured promotion rank.
out=$(run_cmds "position fen 5k3/9/4D4/9/9/9/9/9/9/5K3 w - - 0 1
go perft 1")
echo "${out}" | grep -q "^e8d8c: 1$"
! echo "${out}" | grep -q "^e8d8q: 1$"

# Captured material goes to the capturer's hand like crazyhouse.
out=$(run_cmds "position fen 5k3/9/9/9/4d4/5D3/9/9/9/5K3 w - - 0 1 moves f5e6
d")
echo "${out}" | grep -q "Fen: 5k3/9/9/9/4D4/9/9/9/9/5K3\\[D\\] b - - 0 1"
