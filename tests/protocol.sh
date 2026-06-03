#!/bin/bash
# verify protocol implementations

set -euo pipefail

echo "protocol testing started"

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "protocol testing"

run_expect "$ENGINE" > /dev/null <<EOF
$(expect_engine_setup)
   send "uci\n"
   expect "default chess"
   expect "uciok"
   send "quit\n"
   expect eof
EOF

run_expect "$ENGINE" > /dev/null <<EOF
$(expect_engine_setup)
   send "ucci\n"
   expect "option UCI_Variant"
   expect -re "default (xiangqi|minixiangqi)"
   expect "ucciok"
   send "quit\n"
   expect eof
EOF

run_expect "$ENGINE" > /dev/null <<EOF
$(expect_engine_setup)
   send "usi\n"
   expect -re "default (shogi|minishogi)"
   expect "usiok"
   send "quit\n"
   expect eof
EOF

run_expect "$ENGINE" > /dev/null <<EOF
$(expect_engine_setup)
   send "uci\n"
   expect "uciok"
   send "startpos\n"
   send "d\n"
   expect -re "(rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1|rnbakabnr/9/1c5c1/p1p1p1p/9/9/P1P1P1P/1C5C1/9/RNBAKABNR w - - 0 1)"
   send "quit\n"
   expect eof
EOF

run_expect "$ENGINE" > /dev/null <<EOF
$(expect_engine_setup ucicyclone)
   send "uci\n"
   expect "uciok"
   send "position startpos\n"
   send "d\n"
   expect -re "(rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1|rcnkncr/p1ppp1p/7/7/7/P1PPP1P/RCNKNCR w - - 0 1|rnbakabnr/9/1c5c1/p1p1p1p1p/9/9/P1P1P1P1P/1C5C1/9/RNBAKABNR w - - 0 1)"
   send "quit\n"
   expect eof
EOF

run_expect "$ENGINE" "${VARIANTS}" > /dev/null <<EOF
$(expect_engine_setup 'load [lindex $argv 1]')
   send "xboard\n"
   send "protover 2\n"
   expect "feature done=1"
   send "ping\n"
   expect "pong"
   send "ping\n"
   expect "pong"
   send "variant 3check-crazyhouse\n"
   expect {rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR[] w KQkq - 3+3 0 1}
   send "quit\n"
   expect eof
EOF

echo "protocol testing OK"
