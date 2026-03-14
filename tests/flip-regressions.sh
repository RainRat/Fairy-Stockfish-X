#!/bin/bash

set -euo pipefail

error() {
  echo "flip regression test failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./stockfish}

tmp_ini=$(mktemp /tmp/fsx-flip-XXXXXX.ini)
trap 'rm -f "$tmp_ini"' EXIT

cat >"$tmp_ini" <<'INI'
[flip5:chess]
maxRank = 5
maxFile = e
startFen = 4k/5/5/5/4K w - - 0 1
INI

run_cmds() {
  local cmds="$1"
  cat <<CMDS | "${ENGINE}"
uci
setoption name VariantPath value ${tmp_ini}
setoption name UCI_Variant value flip5
${cmds}
quit
CMDS
}

extract_fen() {
  sed -n 's/^Fen: //p' | tail -n1
}

echo "flip regression tests started"

out=$(run_cmds "position fen 4k/5/5/3Pp/4K w - e3 0 1
flip
d")
fen=$(echo "${out}" | extract_fen)
[[ "${fen}" == "4k/3pP/5/5/4K b - e3 0 1" ]]

echo "flip regression tests passed"
