#!/bin/bash
# verify infinite/ponder waits still stop cleanly after cooperative wait changes

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
ENGINE="${1:-${REPO_ROOT}/src/stockfish}"

error() {
  echo "ponder stop testing failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

run_case() {
  local go_cmd="$1"
  coproc ENGINE_PROC { "$ENGINE"; }
  exec 3>&${ENGINE_PROC[1]} 4<&${ENGINE_PROC[0]}

  printf 'uci\n' >&3
  while IFS= read -r line <&4; do
    [[ "$line" == "uciok" ]] && break
  done

  printf 'position startpos\n%s\n' "$go_cmd" >&3
  sleep 0.2
  printf 'stop\n' >&3

  local got_bestmove=0
  local deadline=$((SECONDS + 10))
  while (( SECONDS < deadline )); do
    if IFS= read -r -t 1 line <&4; then
      if [[ "$line" == bestmove* ]]; then
        got_bestmove=1
        break
      fi
    fi
  done

  (( got_bestmove == 1 ))

  printf 'quit\n' >&3
  wait "$ENGINE_PROC_PID"
  exec 3>&- 4<&-
}

echo "ponder stop testing started"
run_case "go infinite"
run_case "go ponder depth 4"
echo "ponder stop testing OK"