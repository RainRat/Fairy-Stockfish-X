#!/bin/bash

set -euo pipefail

error() {
  echo "xboard regression test failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
ENGINE="${1:-${REPO_ROOT}/src/stockfish}"

run_xboard() {
  cat <<CMDS | "${ENGINE}" xboard
protover 2
$1
quit
CMDS
}

echo "xboard regression tests started"

out=$(run_xboard "level 40 x y")
echo "${out}" | grep -q "feature done=1"

out=$(run_xboard "level 40 5:xx z")
echo "${out}" | grep -q "feature done=1"

out=$(run_xboard "option   Verbosity=2")
echo "${out}" | grep -q "feature done=1"

echo "xboard regression tests passed"