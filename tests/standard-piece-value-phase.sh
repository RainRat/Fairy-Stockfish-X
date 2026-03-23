#!/bin/bash

set -euo pipefail

error() {
  echo "standard piece value phase regression failed on line $1"
  [[ -n "${TMP_VARIANT_PATH:-}" ]] && rm -f "${TMP_VARIANT_PATH}"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./stockfish}

TMP_VARIANT_PATH=$(mktemp /tmp/fsx-piecevalue-phase-XXXXXX.ini)
cat >"${TMP_VARIANT_PATH}" <<'INI'
[knight-low-eg:chess]
pieceValueMg = n:1000
pieceValueEg = n:1
INI

eval_out() {
  cat <<CMDS | "${ENGINE}"
uci
setoption name VariantPath value ${TMP_VARIANT_PATH}
setoption name UCI_Variant value knight-low-eg
position fen 4k3/8/8/3N4/8/8/8/4K3 w - - 0 1
eval
quit
CMDS
}

output=$(eval_out)
material_line=$(printf '%s\n' "${output}" | awk -F'|' '/^[|][[:space:]]+Material[[:space:]]+[|]/ { print $5 }' | tr -s ' ')
[[ -n "${material_line}" ]]

mg=$(printf '%s\n' "${material_line}" | awk '{print $1}')
eg=$(printf '%s\n' "${material_line}" | awk '{print $2}')

python3 - "${mg}" "${eg}" <<'PY'
import sys
mg = float(sys.argv[1])
eg = float(sys.argv[2])
if mg <= 1.0 or eg <= 0.0 or mg <= eg * 5:
    raise SystemExit(f"expected material row to reflect MG/EG override, got mg={mg} eg={eg}")
PY

rm -f "${TMP_VARIANT_PATH}"
unset TMP_VARIANT_PATH

echo "standard piece value phase regression passed"
