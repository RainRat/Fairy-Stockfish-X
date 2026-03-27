#!/bin/bash

set -euo pipefail

error() {
  echo "pushing regression failed on line $1" >&2
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./stockfish}

TMP_INI=$(mktemp)
trap 'rm -f "${TMP_INI}"' EXIT

cat > "${TMP_INI}" <<'INI'
[push-them:fairy]
maxFile = e
maxRank = 5
castling = false
checking = false
startFen = 5/5/5/5/5 w - - 0 1
pushingStrength = r:2
pushFirstColor = them
pushingRemoves = none

[push-us:push-them]
pushFirstColor = us

[push-shove:push-them]
pushingRemoves = shove
INI

run_cmds() {
  local variant=$1
  local cmds=$2
  cat <<EOF | "${ENGINE}" 2>/dev/null
uci
setoption name VariantPath value ${TMP_INI}
setoption name UCI_Variant value ${variant}
${cmds}
quit
EOF
}

out=$(run_cmds push-them "position fen 5/5/5/Rrr2/5 w - - 0 1
go perft 1")
echo "${out}" | grep -q "^a2b2: 1$"

out=$(run_cmds push-them "position fen 5/5/5/Rrrr1/5 w - - 0 1
go perft 1")
! echo "${out}" | grep -q "^a2b2: 1$"

out=$(run_cmds push-us "position fen 5/5/5/RR3/5 w - - 0 1
go perft 1")
echo "${out}" | grep -q "^a2b2: 1$"

out=$(run_cmds push-shove "position fen 5/5/5/2Rrr/5 w - - 0 1 moves c2d2
d")
echo "${out}" | grep -q "Fen: 5/5/5/3Rr/5 b - - 0 1"

echo "pushing ok"
