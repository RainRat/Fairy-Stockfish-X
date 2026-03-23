#!/bin/bash

set -euo pipefail

error() {
  echo "standard piece Betza regression failed on line $1"
  [[ -n "${TMP_VARIANT_PATH:-}" ]] && rm -f "${TMP_VARIANT_PATH}"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./stockfish}

TMP_VARIANT_PATH=$(mktemp /tmp/fsx-standard-piece-betza-XXXXXX.ini)
cat >"${TMP_VARIANT_PATH}" <<'INI'
[rook-full:chess]
king = -
checking = false
rook = r:R
startFen = 8/8/8/8/4R3/8/8/8 w - - 0 1

[rook-short:rook-full]
rook = r:R3
INI

run_cmds() {
  local variant="$1"
  local command="$2"
  cat <<CMDS | "${ENGINE}"
uci
setoption name VariantPath value ${TMP_VARIANT_PATH}
setoption name UCI_Variant value ${variant}
position startpos
${command}
quit
CMDS
}

echo "standard piece Betza regression tests started"

full=$(run_cmds rook-full "go perft 1")
short=$(run_cmds rook-short "go perft 1")

echo "${full}" | grep -q "^e4a4: 1$"
echo "${full}" | grep -q "^e4h4: 1$"
echo "${short}" | grep -q "^e4b4: 1$"
echo "${short}" | grep -q "^e4h4: 1$"
echo "${short}" | grep -q "^e4a4: 1$" && exit 1
echo "${short}" | grep -q "^e4e8: 1$" && exit 1
echo "${short}" | grep -q "^e4e7: 1$"

eval_full=$(cat <<CMDS | "${ENGINE}" | awk '/Final evaluation/ { print $(NF-2) }'
uci
setoption name VariantPath value ${TMP_VARIANT_PATH}
setoption name UCI_Variant value rook-full
position startpos
eval
quit
CMDS
)
eval_short=$(cat <<CMDS | "${ENGINE}" | awk '/Final evaluation/ { print $(NF-2) }'
uci
setoption name VariantPath value ${TMP_VARIANT_PATH}
setoption name UCI_Variant value rook-short
position startpos
eval
quit
CMDS
)

python3 - <<'PY' "${eval_full}" "${eval_short}"
import sys
full = float(sys.argv[1])
short = float(sys.argv[2])
if not full > short:
    raise SystemExit(f"expected full rook eval > short rook eval, got {full} vs {short}")
PY

rm -f "${TMP_VARIANT_PATH}"
unset TMP_VARIANT_PATH

echo "standard piece Betza regression tests passed"
