#!/bin/bash

set -euo pipefail

error() {
  echo "self-capture-types regression failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./stockfish}

TMP_VARIANT_PATH=$(mktemp /tmp/fsx-selfcapture-types-XXXXXX.ini)
trap 'rm -f "${TMP_VARIANT_PATH}"' EXIT

cat >"${TMP_VARIANT_PATH}" <<'INI'
[self-capture-pawn-only:chess]
selfCaptureTypes = p
INI

run_cmds() {
  cat <<EOF | "${ENGINE}" 2>/dev/null
uci
setoption name VariantPath value ${TMP_VARIANT_PATH}
setoption name UCI_Variant value self-capture-pawn-only
$1
quit
EOF
}

out=$(run_cmds "position fen 4k3/8/8/8/8/3Q4/4P3/4K3 w - - 0 1
go perft 1")
echo "${out}" | grep -q "^e2d3: 1$"
! echo "${out}" | grep -q "^d3e2: 1$"

out=$(run_cmds "position fen 4k3/8/8/8/8/3Q4/4P3/4K3 w - - 0 1 moves e2d3
d")
echo "${out}" | grep -Fq "Fen: 4k3/8/8/8/8/3P4/8/4K3 b - - 0 1"

echo "self-capture-types regression passed"
