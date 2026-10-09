#!/bin/bash
# verify protocol implementations

set -euo pipefail

echo "protocol testing started"

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/../../../.."/tests/lib/uci.sh

init_test_env "${1:-}" "${2:-}" "protocol testing"

run_expect "$ENGINE" > /dev/null <<EOF
$(expect_engine_setup)
   send "uci\n"
   expect "default chess"
   expect "uciok"
   send "quit\n"
   expect eof
EOF

# Keep the persistent protocol position attached to a live worker when the
# thread option recreates the pool. ASan catches the stale rootMoves access.
run_expect "$ENGINE" > /dev/null <<EOF
$(expect_engine_setup)
   send "uci\n"
   expect "uciok"
   send "position startpos\n"
   send "setoption name Threads value 2\n"
   send "go perft 2\n"
   expect "Nodes searched: 400"
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
   expect -re "(rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1|rnbakabnr/9/1c5c1/p1p1p1p1p/9/9/P1P1P1P1P/1C5C1/9/RNBAKABNR w - - 0 1)"
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

assert_position_rejected() {
  local command="$1"
  local expected="$2"
  local output
  if output=$(printf 'uci\n%s\n' "$command" | "$ENGINE" 2>&1); then
    echo "expected engine to reject: $command" >&2
    return 1
  fi
  assert_contains_literal "$output" "$expected" "report the rejected input"
}

assert_position_rejected "position startpos garbage" "expected 'moves' after startpos"
assert_position_rejected "position startpos moves e2e4 bogus g1f3" "Invalid move 'bogus'"
assert_position_rejected "position fen not-a-fen" "Invalid position FEN"
# Standard chess cannot reach evaluation with a missing royal or a pocket.
assert_position_rejected "position fen 4k3/8/8/8/8/8/4Q3/8 w - - 0 1" "Invalid number of white kings."
assert_position_rejected "position fen 4k3/8/8/8/8/8/4Q3/4K3[N] w - - 0 1" "Invalid piece character: '['."

# Malformed numeric `go` limits are rejected without killing the engine:
# the command is ignored with an `info string error` diagnostic and the
# engine still answers `isready`.
run_expect "$ENGINE" > /dev/null <<EOF
$(expect_engine_setup)
   send "uci\n"
   expect "uciok"
   send "go depth xyz\n"
   expect "info string error: invalid argument for 'depth'"
   send "isready\n"
   expect "readyok"
   send "go depth -1\n"
   expect "info string error: negative search limit"
   send "isready\n"
   expect "readyok"
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
   expect -ex {rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR[] w KQkq - 3+3 0 1}
   send "quit\n"
   expect eof
EOF

echo "protocol testing OK"
