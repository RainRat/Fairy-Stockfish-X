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
echo "${out}" | grep -q "Error (bad level): level"

out=$(run_xboard $'expect "feature done=1"\nsend "level 40 5:xx z\\n"\n')
echo "${out}" | grep -q "Error (bad level): level"

out=$(run_xboard $'expect "feature done=1"\nsend "usermove\\n"\n')
echo "${out}" | grep -q "Error (bad usermove): usermove"

out=$(run_xboard $'expect "feature done=1"\nsend "sd -4\\n"\nsend "perft 0\\n"\n')
echo "${out}" | grep -q "Error (bad sd): sd"
echo "${out}" | grep -q "Error (bad perft): perft"

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

out=$(run_expect "$ENGINE" <<EOF
$(expect_engine_setup xboard)
   send "protover 2\n"
   expect "feature done=1"
   send "variant chess\n"
   send "setboard 4k3/8/8/8/8/8/8/4K3 w - - 100 51\n"
   send "go\n"
   expect "1/2-1/2 {Draw}"
   send "quit\n"
   expect eof
EOF
)
echo "${out}" | grep -q "1/2-1/2 {Draw}"

out=$(run_expect "$ENGINE" <<EOF
$(expect_engine_setup xboard)
   send "protover 2\n"
   expect "feature done=1"
   send "variant chess\n"
   send "sd 1\n"
   send "setboard 6k1/5Q2/6K1/8/8/8/8/8 w - - 100 51\n"
   send "go\n"
   expect "move "
   send "quit\n"
   expect eof
EOF
)
echo "${out}" | grep -q "move "
if echo "${out}" | grep -q "1/2-1/2 {Draw}"; then
  echo "${out}"
  false
fi

out=$(run_expect "$ENGINE" <<EOF
$(expect_engine_setup xboard)
   send "protover 2\n"
   expect "feature done=1"
   send "variant chess\n"
   send "holding \\[R\\] \\[r\\] bQ\n"
   send "d\n"
   send "quit\n"
   expect eof
EOF
)
echo "${out}" | grep -q "Fen: rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w"

echo "xboard regression tests passed"
