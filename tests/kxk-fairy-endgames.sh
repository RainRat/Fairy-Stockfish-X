#!/bin/bash

source "$(dirname "$0")/common.sh"

echo "kxk-fairy-endgames test started"

tmp_ini=$(create_tmp_ini <<'INI'
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

[kxk-bers:chess]
bers = b
pieceToCharTable = PNBRQ.....B..........Kpnbrq.....b..........k
startFen = 4k3/8/8/8/8/8/8/4K2B w - - 0 1
castling = false
doubleStep = false

[kxk-aiwok:chess]
aiwok = i
pieceToCharTable = PNBRQ....I...........Kpnbrq....i...........k
startFen = 4k3/8/8/8/8/8/8/4K2I w - - 0 1
castling = false
doubleStep = false
INI
)

check_eval() {
  local variant=$1
  local output
  local score

  output=$(run_uci "setoption name UCI_Variant value ${variant}\nposition startpos\neval" "${tmp_ini}")

  score=$(printf '%s\n' "${output}" | awk '/Final evaluation/ {print $(NF-2)}' | tail -n1)
  if [ -z "${score}" ]; then
    echo "missing eval for ${variant}"
    printf '%s\n' "${output}"
    exit 1
  fi

  # Simple score check using python3 (bc is missing in this environment)
  python3 -c "if float('${score}') < 50.0: exit(1)"
}

check_eval kxk-arch
check_eval kxk-chanc
check_eval kxk-amazon
check_eval kxk-bers
check_eval kxk-aiwok

echo "kxk-fairy-endgames test OK"
