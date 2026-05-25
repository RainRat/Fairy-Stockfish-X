#!/bin/bash

set -euo pipefail

error() {
  echo "janggi regression failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-src/stockfish}

run_cmds() {
  cat <<EOF | "${ENGINE}"
uci
$1
quit
EOF
}

variant_available() {
  local out
  out=$(printf 'uci\nquit\n' | "${ENGINE}")
  grep -q ' var janggi' <<<"${out}"
}

if ! variant_available; then
  echo "janggi variant not available in this build; skipping janggi regression"
  exit 0
fi

out=$(run_cmds "setoption name UCI_Variant value janggi
position startpos
go perft 1")
grep -Fxq "Nodes searched: 32" <<<"${out}"
grep -Fxq "0000: 1" <<<"${out}"

out=$(run_cmds "setoption name UCI_Variant value janggi
position fen 1n1kaabn1/cr2N4/5C1c1/p1pNp3p/9/9/P1PbP1P1P/3r1p3/4A4/R1BA1KB1R b - - 0 1 moves a9e9 e2d3
go perft 1")
grep -Fxq "Nodes searched: 37" <<<"${out}"
grep -Fxq "f3e2: 1" <<<"${out}"
grep -Fxq "0000: 1" <<<"${out}"

echo "janggi regression tests passed"
