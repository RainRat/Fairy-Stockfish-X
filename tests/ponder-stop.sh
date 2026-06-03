#!/bin/bash
# verify infinite/ponder waits still stop cleanly after cooperative wait changes

set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "ponder stop testing"

run_case() {
  local go_cmd="$1"
  run_expect "$ENGINE" <<EOF
$(expect_engine_setup)
   set timeout 10
   send "uci\n"
   expect "uciok"
   send "position startpos\n"
   send "$go_cmd\n"
   after 200
   send "stop\n"
   expect -re {^bestmove\b}
   send "quit\n"
   expect eof
EOF
}

echo "ponder stop testing started"
run_case "go infinite"
run_case "go ponder depth 4"
echo "ponder stop testing OK"
