#!/bin/bash
# verify reproducible search

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/../../../.."/tests/lib/uci.sh

init_test_env "${1:-}" "${2:-}" "reprosearch testing"

echo "reprosearch testing started"
log=$(mktemp)
trap 'rm -f "$log"' EXIT

# repeat two short games, separated by ucinewgame.
# with go nodes $nodes they should result in exactly
# the same node count for each iteration.
for i in `seq 1 20`
do
  nodes=$((100*3**i/2**i))
  echo "reprosearch testing with $nodes nodes"

  if ! EXPECT_TIMEOUT=120 run_expect "$ENGINE" "$nodes" >"$log" 2>&1 <<EOF
$(expect_engine_setup)
 set timeout 30
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
  then
    cat "$log"
    exit 1
  fi

  # Intermediate aspiration-window reports depend on elapsed wall time.
  # Compare the final node count of each search, preserving game order.
  if ! awk '
    /^info / {
      for (i = 1; i < NF; ++i)
        if ($i == "nodes") nodes = $(i + 1)
    }
    /^bestmove / { counts[++searches] = nodes; nodes = "" }
    END {
      if (searches != 4 || counts[1] == "" || counts[2] == "" ||
          counts[1] != counts[3] || counts[2] != counts[4]) exit 1
    }
  ' "$log"; then
    cat "$log"
    exit 1
  fi
done

echo "reprosearch testing OK"
