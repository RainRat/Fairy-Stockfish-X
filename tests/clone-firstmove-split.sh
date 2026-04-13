#!/bin/bash

set -euo pipefail

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
[clone-firstmove-split:fairy]
customPiece1 = a:R
gating = true
castling = false
firstMovePieceTypes = a:b
cloneMoveTypes = a
startFen = 7k/8/8/8/8/8/8/K2A4 w D - 0 1
INI

out=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value clone-firstmove-split\nposition startpos\ngo perft 1\nquit\n' "${tmp_ini}" | "${ENGINE}")
echo "${out}" | grep -q "^d1e2: 1$"
echo "${out}" | grep -q "^d1d2c: 1$"
! echo "${out}" | grep -q "^d1e2c: 1$"

move_out=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value clone-firstmove-split\nposition startpos moves d1e2\nd\nquit\n' "${tmp_ini}" | "${ENGINE}")
echo "${move_out}" | grep -q "Fen: 7k/8/8/8/8/8/4A3/K7 b - - 1 1"

clone_out=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value clone-firstmove-split\nposition startpos moves d1d2c\nd\nquit\n' "${tmp_ini}" | "${ENGINE}")
echo "${clone_out}" | grep -q "Fen: 7k/8/8/8/8/8/3A4/K2A4 b - - 1 1"
