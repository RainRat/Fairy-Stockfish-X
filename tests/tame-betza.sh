#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")/../src"

tmp_ini=$(mktemp)
trap 'rm -f "$tmp_ini"' EXIT

cat > "$tmp_ini" <<'INI'
[tame-check:chess]
customPiece1 = d:Qt
pieceToCharTable = PNBRQ............D...Kpnbrq............d...k
startFen = 4k3/4D3/8/8/8/8/8/4K3 b - - 0 1

[untame-check:chess]
customPiece1 = d:Q
pieceToCharTable = PNBRQ............D...Kpnbrq............d...k
startFen = 4k3/4D3/8/8/8/8/8/4K3 b - - 0 1

[tame-capture:chess]
customPiece1 = d:Qt
pieceToCharTable = PNBRQ............D...Kpnbrq............d...k
startFen = k7/8/8/8/8/8/8/D3K3 w - - 0 1
checking = false

[untame-capture:chess]
customPiece1 = d:Q
pieceToCharTable = PNBRQ............D...Kpnbrq............d...k
startFen = k7/8/8/8/8/8/8/D3K3 w - - 0 1
checking = false
INI

run_cmds() {
  local variant=$1
  local cmds=$2
  cat <<CMDS | ./stockfish
uci
setoption name VariantPath value ${tmp_ini}
setoption name UCI_Variant value ${variant}
${cmds}
quit
CMDS
}

untame_check=$(run_cmds untame-check "position startpos
d")
echo "${untame_check}" | grep -q "^Checkers: e7 "

tame_check=$(run_cmds tame-check "position startpos
d")
if echo "${tame_check}" | grep -q "^Checkers: [^ ]"; then
  echo "tame check test failed: tame queen should not give check"
  exit 1
fi

untame_capture=$(run_cmds untame-capture "position startpos
go perft 1")
echo "${untame_capture}" | grep -q "^a1a8: 1$"

tame_capture=$(run_cmds tame-capture "position startpos
go perft 1")
if echo "${tame_capture}" | grep -q "^a1a8: 1$"; then
  echo "tame capture test failed: tame queen should not capture king"
  exit 1
fi

echo "tame-betza test OK"
