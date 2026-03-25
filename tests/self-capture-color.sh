#!/bin/bash

set -euo pipefail

error() {
  echo "self-capture-color regression failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./stockfish}

TMP_VARIANT_PATH=$(mktemp /tmp/fsx-selfcapture-color-XXXXXX.ini)
trap 'rm -f "${TMP_VARIANT_PATH}"' EXIT

cat >"${TMP_VARIANT_PATH}" <<'INI'
[self-capture-black-only:chess]
selfCaptureBlack = true
INI

run_cmds() {
  cat <<EOF | "${ENGINE}" 2>/dev/null
uci
setoption name VariantPath value ${TMP_VARIANT_PATH}
setoption name UCI_Variant value self-capture-black-only
$1
quit
EOF
}

out=$(run_cmds "position fen 4k3/8/8/8/8/8/4Q3/3RK3 w - - 0 1
go perft 1")
! echo "${out}" | grep -q "^d1e2: 1$"

out=$(run_cmds "position fen 3qk3/4r3/8/8/8/8/8/4K3 b - - 0 1
go perft 1")
echo "${out}" | grep -q "^d8e7: 1$"
