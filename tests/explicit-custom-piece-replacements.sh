#!/bin/bash

set -euo pipefail

error() {
  echo "explicit custom piece replacement regression failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./stockfish}
VARIANTS=${2:-src/variants.ini}

run_perft() {
  local variant="$1"
  local fen="$2"
  cat <<CMDS | "${ENGINE}"
uci
setoption name VariantPath value ${VARIANTS}
setoption name UCI_Variant value ${variant}
position fen ${fen}
go perft 1
quit
CMDS
}

echo "explicit custom piece replacement regression tests started"

# British bishop = BmW on 10x10.
out=$(run_perft "british-chess" "4q5/10/10/10/4B5/10/10/10/10/4Q5 w - - 0 1")
echo "${out}" | grep -q "^e6e7: 1$"
echo "${out}" | grep -q "^e6f7: 1$"
echo "${out}" | grep -q "^e6j6: 1$" && exit 1

# Compound rook = RA on 10x8.
out=$(run_perft "compound-chess" "4k5/10/10/4R5/10/10/10/4K5 w - - 0 1")
echo "${out}" | grep -q "^e5e7: 1$"
echo "${out}" | grep -q "^e5e8: 1$"
echo "${out}" | grep -q "^e5g7: 1$"
echo "${out}" | grep -q "^e5h8: 1$" && exit 1

# Chaturanga al-Adli bishop = D on 8x8.
out=$(run_perft "chaturanga-al-adli" "rnbfk1nr/pppppppp/8/8/3B4/8/PPPPPPPP/RN1FK1NR w - - 0 1")
echo "${out}" | grep -q "^d4d6: 1$"
echo "${out}" | grep -q "^d4f4: 1$"
echo "${out}" | grep -q "^d4f6: 1$" && exit 1

echo "explicit custom piece replacement regression tests passed"
