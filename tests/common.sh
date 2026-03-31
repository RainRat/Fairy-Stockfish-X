#!/bin/bash

# common.sh: shared functions for shell tests

set -euo pipefail

# Setup repo root and paths
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
export FSX_REPO_ROOT="${REPO_ROOT}"

# Find stockfish binary
if [[ -f "${REPO_ROOT}/src/stockfish" ]]; then
  STOCKFISH="${REPO_ROOT}/src/stockfish"
elif [[ -f "${REPO_ROOT}/src/stockfish.exe" ]]; then
  STOCKFISH="${REPO_ROOT}/src/stockfish.exe"
else
  STOCKFISH="stockfish"
fi

# Common error handler
error() {
  local script_name=$(basename "$0")
  echo "${script_name} failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

# Utility to run uci commands
# Usage: run_uci "commands" [variant_path]
run_uci() {
  local cmds=""
  if [[ -n "${2:-}" ]]; then
    cmds="setoption name VariantPath value ${2}\n"
  fi
  cmds="${cmds}${1}\nquit\n"
  printf "${cmds}" | "${STOCKFISH}"
}

# Cleanup temporary files automatically
tmp_files=()
cleanup() {
  for f in "${tmp_files[@]}"; do
    rm -f "$f"
  done
}
trap "cleanup; error \$LINENO" ERR
trap "cleanup" EXIT

add_tmp_file() {
  tmp_files+=("$1")
}

create_tmp_ini() {
  local f=$(mktemp)
  add_tmp_file "$f"
  cat > "$f"
  echo "$f"
}
