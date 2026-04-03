#!/bin/bash

set -euo pipefail

error() {
  echo "spell freeze regression failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
ENGINE="${1:-${REPO_ROOT}/src/stockfish}"
DEFAULT_VARIANT_PATH="${REPO_ROOT}/src/variants.ini"
if [[ ! -f "${DEFAULT_VARIANT_PATH}" && -f "src/${REPO_ROOT}/src/variants.ini" ]]; then
  DEFAULT_VARIANT_PATH="src/${REPO_ROOT}/src/variants.ini"
fi
VARIANT_PATH=${2:-${DEFAULT_VARIANT_PATH}}

run_cmds() {
  cat <<CMDS | "${ENGINE}"
uci
setoption name VariantPath value ${VARIANT_PATH}
$1
quit
CMDS
}

echo "spell freeze regression tests started"

# Frozen castling rook cannot participate in castling.
out=$(run_cmds "setoption name UCI_Variant value spell-chess
position fen 4k3/8/8/8/8/8/8/4K2R[f] b K - 0 1 moves f@h1 e8e7
go perft 1")
! echo "${out}" | grep -q "^e1g1:"

# Castling through attack remains illegal unless the checker is frozen first.
out=$(run_cmds "setoption name UCI_Variant value spell-chess
position fen 4kr2/8/8/8/8/8/8/4K2R[F] w K - 0 1
go perft 1")
! echo "${out}" | grep -q "^e1g1:"
echo "${out}" | grep -q "^f@f8,e1g1: 1$"

# Castling out of check remains illegal unless the checking rook is frozen first.
out=$(run_cmds "setoption name UCI_Variant value spell-chess
position fen 4r1k1/8/8/8/8/8/8/4K2R[F] w K - 0 1
go perft 1")
! echo "${out}" | grep -q "^e1g1:"
echo "${out}" | grep -q "^f@e8,e1g1: 1$"

# Jump potion does not make castling legal through occupied blocker squares.
out=$(run_cmds "setoption name UCI_Variant value spell-chess
position fen 6k1/8/8/8/8/8/8/R2nK3[J] w Q - 0 1
go perft 1")
! echo "${out}" | grep -q "^e1c1:"
! echo "${out}" | grep -q "^j@d1,e1c1:"

out=$(run_cmds "setoption name UCI_Variant value spell-chess
position fen 6k1/8/8/8/8/8/8/Rn2K3[J] w Q - 0 1
go perft 1")
! echo "${out}" | grep -q "^e1c1:"
! echo "${out}" | grep -q "^j@b1,e1c1:"

# Frozen pawns cannot capture en passant.
out=$(run_cmds "setoption name UCI_Variant value spell-chess
position fen 4k3/3p4/8/4P3/8/8/8/4K3[f] b - - 0 1 moves f@e5 d7d5
go perft 1")
! echo "${out}" | grep -q "^e5d6:"

echo "spell freeze regression tests passed"