#!/bin/bash

set -euo pipefail

ENGINE=${1:-src/stockfish}
VARIANT_PATH=${2:-}

tmp_ini=
cleanup() {
  if [[ -n "${tmp_ini}" ]]; then
    rm -f "${tmp_ini}"
  fi
}
trap cleanup EXIT

if [[ -z "${VARIANT_PATH}" ]]; then
  tmp_ini=$(mktemp)
  cat > "${tmp_ini}" <<'EOF'
[battery-chess:chess]
captureType = hand
pieceDrops = false
promotionRequireInHand = true
promotionConsumeInHand = true
EOF
  VARIANT_PATH="${tmp_ini}"
fi

run_cmds() {
  cat <<EOF | "${ENGINE}"
uci
setoption name VariantPath value ${VARIANT_PATH}
$1
quit
EOF
}

echo "battery-chess test started"

out=$(run_cmds "setoption name UCI_Variant value battery-chess
position fen 4k3/P7/8/8/8/8/8/4K3 w - - 0 1
go perft 1")
! echo "${out}" | grep -q "^a7a8"

out=$(run_cmds "setoption name UCI_Variant value battery-chess
position fen 4k3/P7/8/8/8/8/8/4K3[Q] w - - 0 1
go perft 1")
echo "${out}" | grep -q "^a7a8q: 1$"
! echo "${out}" | grep -q "^a7a8n:"
! echo "${out}" | grep -q "^a7a8r:"
! echo "${out}" | grep -q "^a7a8b:"

out=$(run_cmds "setoption name UCI_Variant value battery-chess
position fen 4k3/P7/8/8/8/8/8/4K3[Q] w - - 0 1 moves a7a8q
d")
echo "${out}" | grep -q "Fen: Q~3k3/8/8/8/8/8/8/4K3\\[\\] b - - 0 1"

echo "battery-chess test OK"
