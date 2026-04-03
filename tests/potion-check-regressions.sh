#!/bin/bash

set -euo pipefail

error() {
  echo "potion check regression failed on line $1"
  [[ -n "${TMP_VARIANT_PATH:-}" ]] && rm -f "${TMP_VARIANT_PATH}"
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
ENGINE="${1:-${REPO_ROOT}/src/stockfish}"

TMP_VARIANT_PATH=$(mktemp /tmp/fsx-potioncheck-XXXXXX.ini)
cat >"${TMP_VARIANT_PATH}" <<'INI'
[potioncheck:chess]
potions = true
freezePotion = r
potionDropOnOccupied = true
checking = false
startFen = 4k3/8/8/8/8/8/8/4K3[R] w - - 0 1
INI

run_cmds() {
  cat <<CMDS | "${ENGINE}"
uci
setoption name VariantPath value ${TMP_VARIANT_PATH}
setoption name UCI_Variant value potioncheck
$1
quit
CMDS
}

echo "potion check regression tests started"

out=$(run_cmds "position startpos
go perft 1")
echo "${out}" | grep -q "^r@d8,e1d1: 1$"
echo "${out}" | grep -q "^r@f8,e1f2: 1$"
echo "${out}" | grep -q "^r@e8,e1e2: 1$"

rm -f "${TMP_VARIANT_PATH}"
unset TMP_VARIANT_PATH

echo "potion check regression tests passed"