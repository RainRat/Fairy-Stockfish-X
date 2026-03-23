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

extract_final_eval() {
  sed -n 's/^Final evaluation[[:space:]]*//p' | tail -n1 | awk '{print $1}'
}

output=$(eval_out)
score=$(printf '%s\n' "${output}" | extract_final_eval)

[[ -n "${score}" ]]

python3 - "${score}" <<'PY'
import sys
score = float(sys.argv[1])
if score <= 0.10:
    raise SystemExit(f"expected endgame-weighted positive score, got {score}")
PY

rm -f "${TMP_VARIANT_PATH}"
unset TMP_VARIANT_PATH

echo "standard piece value phase regression passed"
