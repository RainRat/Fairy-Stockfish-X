#!/bin/bash
# verify reproducible search

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "reprosearch testing"

echo "reprosearch testing started"

# repeat two short games, separated by ucinewgame.
# with go nodes $nodes they should result in exactly
# the same node count for each iteration.
for i in `seq 1 20`
do
  nodes=$((100*3**i/2**i))
  echo "reprosearch testing with $nodes nodes"

  run_expect "$ENGINE" "$nodes" 2>&1 <<EOF | grep -o "nodes [0-9]*" | sort | uniq -c | awk '{if ($1%2!=0) exit(1)}'
$(expect_engine_setup)
 set timeout 10
 lassign [lrange \$argv 1 end] nodes

   send "uci\n"
   expect "uciok"

 send "ucinewgame\n"
 send "position startpos\n"
 send "go nodes \$nodes\n"
 expect "bestmove"

 send "position startpos moves e2e4 e7e6\n"
 send "go nodes \$nodes\n"
 expect "bestmove"

 send "ucinewgame\n"
 send "position startpos\n"
 send "go nodes \$nodes\n"
 expect "bestmove"

 send "position startpos moves e2e4 e7e6\n"
 send "go nodes \$nodes\n"
 expect "bestmove"

 send "quit\n"
 expect eof
EOF
done

echo "reprosearch testing OK"
