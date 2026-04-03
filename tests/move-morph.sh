#!/bin/bash

set -euo pipefail

error() {
  echo "move-morph regression failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
ENGINE="${1:-${REPO_ROOT}/src/stockfish}"
VARIANTS=${2:-src/${REPO_ROOT}/src/variants.ini}

run_cmds() {
  cat <<EOF | "${ENGINE}" 2>/dev/null
uci
setoption name VariantPath value ${VARIANTS}
setoption name UCI_Variant value bishop-knight-morph-factor
$1
quit
EOF
}

out=$(run_cmds "position startpos moves g1f3
d")
echo "${out}" | grep -q "Fen: rnbqkbnr/pppppppp/8/8/8/5B2/PPPPPPPP/RNBQKB1R b KQkq - 1 1"

out=$(run_cmds "position fen 4k3/8/8/8/8/8/8/2B1K3 w - - 0 1 moves c1g5
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/6N1/8/8/8/4K3 b - - 1 1"