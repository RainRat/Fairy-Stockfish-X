#!/bin/bash

set -euo pipefail

error() {
  echo "slide-5 regression failed on line $1" >&2
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ENGINE="${1:-${SCRIPT_DIR}/../src/stockfish}"
VARIANT_PATH="${2:-${SCRIPT_DIR}/../src/variants.ini}"

perft_out=$(cat <<EOF | "${ENGINE}" 2>/dev/null
uci
setoption name Hash value 1
setoption name Clear Hash
setoption name VariantPath value ${VARIANT_PATH}
setoption name UCI_Variant value slide-5
position startpos
go perft 1
position startpos moves A@a1,b1
go perft 1
quit
EOF
)

echo "${perft_out}" | grep -q "Nodes searched: 10"

tty_out=$(expect <<EOF
log_user 1
set timeout 10
spawn ${ENGINE}
expect "by Fabian Fichter"
send "setoption name Hash value 1\r"
send "setoption name Clear Hash\r"
send "setoption name VariantPath value ${VARIANT_PATH}\r"
send "setoption name UCI_Variant value slide-5\r"
send "position startpos\r"
send "go movetime 10\r"
expect -re "bestmove .*"
send "position startpos moves A@a1,b1\r"
send "go movetime 10\r"
expect -re "bestmove .*"
send "quit\r"
expect eof
EOF
)

echo "${tty_out}" | grep -q "^info depth "
echo "${tty_out}" | grep -q "^bestmove "
! echo "${tty_out}" | grep -q "score mate"
! echo "${tty_out}" | grep -q "^bestmove A@e5,e4$"

echo "slide-5 regression passed"
