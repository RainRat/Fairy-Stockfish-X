#!/bin/bash

set -euo pipefail

error() {
  echo "variant-switch-after-perft regression failed on line $1"
  [[ -n "${TMP_VARIANT_PATH:-}" ]] && rm -f "${TMP_VARIANT_PATH}"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./stockfish}

TMP_VARIANT_PATH=$(mktemp /tmp/fsx-variant-switch-XXXXXX.ini)
cat >"${TMP_VARIANT_PATH}" <<'INI'
[v1:chess]
startFen = 4k3/8/8/8/8/8/8/4K3 w - - 0 1

[v2:v1]
startFen = 4k3/8/8/8/4P3/8/8/4K3 w - - 0 1
INI

out=$(cat <<CMDS | "${ENGINE}"
uci
setoption name VariantPath value ${TMP_VARIANT_PATH}
setoption name UCI_Variant value v1
position startpos
go perft 1
setoption name UCI_Variant value v2
quit
CMDS
)

echo "${out}" | grep -q "^e1d1: 1$"
echo "${out}" | grep -q "info string variant v2 files 8 ranks 8 pocket 0 template fairy startpos 4k3/8/8/8/4P3/8/8/4K3 w - - 0 1"

rm -f "${TMP_VARIANT_PATH}"
unset TMP_VARIANT_PATH

echo "variant-switch-after-perft regression tests passed"
