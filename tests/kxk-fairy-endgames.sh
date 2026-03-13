#!/bin/bash

set -euo pipefail

ENGINE=${1:-./stockfish}

tmp_ini=$(mktemp)
trap 'rm -f "${tmp_ini}"' EXIT

cat > "${tmp_ini}" <<'INI'
[kxk-arch:chess]
archbishop = a
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 4k3/8/8/8/8/8/8/4K2A w - - 0 1
castling = false
doubleStep = false

[kxk-chanc:chess]
chancellor = c
pieceToCharTable = PNBRQ.............C..Kpnbrq.............c..k
startFen = 4k3/8/8/8/8/8/8/4K2C w - - 0 1
castling = false
doubleStep = false

[kxk-amazon:chess]
amazon = m
pieceToCharTable = PNBRQ..........M....Kpnbrq..........m....k
startFen = 4k3/8/8/8/8/8/8/4K2M w - - 0 1
castling = false
doubleStep = false
INI

check_eval() {
  local variant=$1
  local output
  local score

  output=$(cat <<CMDS | "${ENGINE}" 2>&1
uci
setoption name VariantPath value ${tmp_ini}
setoption name UCI_Variant value ${variant}
position startpos
eval
quit
CMDS
)

  score=$(printf '%s\n' "${output}" | awk '/Final evaluation/ {print $(NF-2)}' | tail -n1)
  if [ -z "${score}" ]; then
    echo "missing eval for ${variant}"
    printf '%s\n' "${output}"
    exit 1
  fi

  python3 - <<PY
score = float("${score}")
if score < 50.0:
    raise SystemExit(1)
PY
}

check_eval kxk-arch
check_eval kxk-chanc
check_eval kxk-amazon
