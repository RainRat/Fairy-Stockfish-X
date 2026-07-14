#!/bin/bash

set -euo pipefail

error() {
  echo "spell freeze regression failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "spell freeze regression"
DEFAULT_VARIANT_PATH="variants.ini"
if [[ ! -f "${DEFAULT_VARIANT_PATH}" && -f "src/variants.ini" ]]; then
  DEFAULT_VARIANT_PATH="src/variants.ini"
fi
VARIANT_PATH=${2:-${DEFAULT_VARIANT_PATH}}

if ! probe_variant_available "$ENGINE" spell-chess "$VARIANT_PATH"; then
  echo "spell freeze regressions skipped (spell-chess requires all=yes)"
  exit 0
fi

run_cmds() {
  run_uci "$ENGINE" "$VARIANT_PATH" spell-chess <<EOF
$1
EOF
}

echo "spell freeze regression tests started"

# Frozen castling rook cannot participate in castling.
out=$(run_cmds "position fen 4k3/8/8/8/8/8/8/4K2R[f] b K - 0 1 moves f@h1 e8e7
go perft 1")
! echo "${out}" | grep -q "^e1g1:"

# Castling through attack remains illegal unless the checker is frozen first.
out=$(run_cmds "position fen 4kr2/8/8/8/8/8/8/4K2R[F] w K - 0 1
go perft 1")
! echo "${out}" | grep -q "^e1g1:"
echo "${out}" | grep -q "^f@f8,e1g1: 1$"

# Castling out of check remains illegal unless the checking rook is frozen first.
out=$(run_cmds "position fen 4r1k1/8/8/8/8/8/8/4K2R[F] w K - 0 1
go perft 1")
! echo "${out}" | grep -q "^e1g1:"
echo "${out}" | grep -q "^f@e8,e1g1: 1$"

# Jump potion does not make castling legal through occupied blocker squares.
out=$(run_cmds "position fen 6k1/8/8/8/8/8/8/R2nK3[J] w Q - 0 1
go perft 1")
! echo "${out}" | grep -q "^e1c1:"
! echo "${out}" | grep -q "^j@d1,e1c1:"

out=$(run_cmds "position fen 6k1/8/8/8/8/8/8/Rn2K3[J] w Q - 0 1
go perft 1")
! echo "${out}" | grep -q "^e1c1:"
! echo "${out}" | grep -q "^j@b1,e1c1:"

# Frozen pawns cannot capture en passant.
out=$(run_cmds "position fen 4k3/3p4/8/4P3/8/8/8/4K3[f] b - - 0 1 moves f@e5 d7d5
go perft 1")
! echo "${out}" | grep -q "^e5d6:"

# Same-turn cast Freeze on own piece prevents that piece from moving.
out=$(run_cmds "position fen rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR[F] w KQkq - 0 1
go perft 1")
! echo "${out}" | grep -q "^f@g3,f2f3:"
! echo "${out}" | grep -q "^f@g3,f2f4:"

# Same-turn cast Freeze on own royal prevents royal moves and castling.
out=$(run_cmds "position fen 4k3/8/8/8/8/8/8/4K2R[F] w K - 0 1
go perft 1")
! echo "${out}" | grep -q "^f@e1,e1"

# Blocked pawn on rank 2 can double-step to rank 4 if blocker is treated with Jump potion.
out=$(run_cmds "position fen 4k3/8/8/8/8/4p3/4P3/4K3[J] w - - 0 1
go perft 1")
echo "${out}" | grep -q "^j@e3,e2e4: 1$"

# Potion cooldown prevents casting the same potion type.
out=$(run_cmds "position fen rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR[F] w KQkq - 0 1 <2 0 0 0>
go perft 1")
! echo "${out}" | grep -q "^f@"

# A Freeze potion may target the square on which its accompanying move lands.
out=$(run_cmds "position startpos moves f@c7,e2e3 f@e1,e7e6 b2b3 g8h6 d2d3 d7d5 d1h5 f@g4,h6g8 f@g8,c1a3 e8d7 h5f7 d7c6 g1h3 f@a2,g8h6
go perft 1")
echo "${out}" | grep -q "^f@c7,f7c7: 1$"

# A Freeze potion does not block an accompanying piece's path through its target.
out=$(run_cmds "position fen 7k/8/8/8/5Q2/8/8/K7[F] w - - 0 1
go perft 1")
echo "${out}" | grep -q "^f@f6,f4f7: 1$"

echo "spell freeze regression tests passed"
