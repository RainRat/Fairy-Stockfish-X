#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
ENGINE="${1:-${REPO_ROOT}/src/stockfish}"
VARIANTS="${2:-./src/${REPO_ROOT}/src/variants.ini}"

die() {
  echo "rebel-chess regression failed on line $1" >&2
  exit 1
}
trap 'die $LINENO' ERR

run_cmds() {
  {
    echo "setoption name VariantPath value ${VARIANTS}"
    echo "setoption name UCI_Variant value rebel-chess"
    printf '%s\n' "$1"
    echo quit
  } | "${ENGINE}"
}

# Sith Master is king+knight, so queen-like sliding is illegal while knight jumps remain.
out=$(run_cmds "position fen 4s3/8/8/8/8/8/8/4K3 b - - 0 1
go perft 1")
! echo "${out}" | grep -q "^e8e6: 1$"
echo "${out}" | grep -q "^e8f6: 1$"

# Black may still capture its own Apprentice.
out=$(run_cmds "position fen 3qs3/8/8/8/8/8/8/4K3 b - - 0 1
go perft 1")
echo "${out}" | grep -q "^e8d8: 1$"