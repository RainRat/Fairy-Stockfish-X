#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "xboard regression test"

run_xboard() {
  local commands="$1"
  run_expect "$ENGINE" <<EOF
$(expect_engine_setup xboard)
   send "protover 2\n"
   $commands
   send "quit\n"
   expect eof
EOF
}

echo "xboard regression tests started"

out=$(run_xboard $'expect "feature done=1"\nsend "level 40 x y\\n"\n')
echo "${out}" | grep -q "feature done=1"

out=$(run_xboard $'expect "feature done=1"\nsend "level 40 5:xx z\\n"\n')
echo "${out}" | grep -q "feature done=1"

out=$(run_xboard $'expect "feature done=1"\nsend "option   Verbosity=2\\n"\n')
echo "${out}" | grep -q "feature done=1"

out=$(run_expect "$ENGINE" <<EOF
$(expect_engine_setup xboard)
   send "protover 2\n"
   expect "feature done=1"
   send "variant isolation\n"
   send "setboard 2*3/2**2/*1p3/6/6/1****1/***P*1/**1*** b - - 17 9\n"
   send "usermove c6d6,d6f2\n"
   send "d\n"
   send "quit\n"
   expect eof
EOF
)
if echo "${out}" | grep -q "Illegal move: c6d6,d6f2"; then
  echo "${out}"
  false
fi
echo "${out}" | grep -q "Fen: 2\\*3/2\\*\\*2/\\*2p2/6/6/1\\*\\*\\*\\*1/\\*\\*\\*P\\*\\*/\\*\\*1\\*\\*\\* w - - 18 10"

out=$(run_expect "$ENGINE" <<EOF
$(expect_engine_setup xboard)
   send "protover 2\n"
   expect "feature done=1"
   send "level 40 999999999999:59 999999999999\n"
   send "time 2147483647\n"
   send "otim 2147483647\n"
   send "st 2147483647\n"
   send "holding \\[R\\] \\[r\\] bQ\n"
   send "quit\n"
   expect eof
EOF
)
echo "${out}" | grep -q "feature done=1"

echo "xboard regression tests passed"
