#!/bin/bash

set -euo pipefail

error() {
  echo "xboard regression test failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./stockfish}

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

out=$(cat <<'CMDS' | "${ENGINE}" xboard
protover 2
variant isolation
setboard 2*3/2**2/*1p3/6/6/1****1/***P*1/**1*** b - - 17 9
usermove c6d6,d6f2
d
quit
CMDS
)
if echo "${out}" | grep -q "Illegal move: c6d6,d6f2"; then
  echo "${out}"
  false
fi
echo "${out}" | grep -q "Fen: 2\\*3/2\\*\\*2/\\*2p2/6/6/1\\*\\*\\*\\*1/\\*\\*\\*P\\*\\*/\\*\\*1\\*\\*\\* w - - 18 10"

echo "xboard regression tests passed"
