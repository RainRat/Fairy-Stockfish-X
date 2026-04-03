#!/bin/bash

set -euo pipefail

error() {
  echo "explicit custom piece replacement regression failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
ENGINE="${1:-${REPO_ROOT}/src/stockfish}"
VARIANTS=${2:-src/${REPO_ROOT}/src/variants.ini}

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

variant_available() {
  local variant="$1"
  cat <<CMDS | "${ENGINE}" | grep -q " var ${variant}\$"
uci
setoption name VariantPath value ${VARIANTS}
quit
CMDS
}

echo "explicit custom piece replacement regression tests started"

# British bishop = BmW on 10x10.
if variant_available "british-chess"; then
  out=$(run_perft "british-chess" "4q5/10/10/10/4B5/10/10/10/10/4Q5 w - - 0 1")
  echo "${out}" | grep -q "^e6e7: 1$"
  echo "${out}" | grep -q "^e6f7: 1$"
  echo "${out}" | grep -q "^e6j6: 1$" && exit 1
fi

# Chaturanga al-Adli bishop = D on 8x8.
out=$(run_perft "chaturanga-al-adli" "rnbfk1nr/pppppppp/8/8/3B4/8/PPPPPPPP/RN1FK1NR w - - 0 1")
echo "${out}" | grep -q "^d4d6: 1$"
echo "${out}" | grep -q "^d4f4: 1$"
echo "${out}" | grep -q "^d4f6: 1$" && exit 1

echo "explicit custom piece replacement regression tests passed"