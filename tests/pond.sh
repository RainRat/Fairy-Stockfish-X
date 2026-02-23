#!/bin/bash
# Pond regression tests (line removal, terminal adjudication, search stability)

set -euo pipefail

error() {
  echo "pond testing failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./stockfish}

echo "pond testing started"

run_uci() {
  local cmd_file
  cmd_file=$(mktemp)
  cat > "$cmd_file"
  local out
  out=$(mktemp)
  timeout 20s "$ENGINE" < "$cmd_file" > "$out" 2>&1
  rm -f "$cmd_file"
  cat "$out"
  rm -f "$out"
}

# 1) Crash regression for deep-ish search from known failing position
output=$(run_uci <<'CMDS'
uci
setoption name VariantPath value variants.ini
setoption name UCI_Variant value pond
position fen 1tt1/T1et/2TF/T1ef[EEEEEEeeeee] w - - 0 11 {3 2}
go movetime 1000
quit
CMDS
)

echo "$output" | grep -q "bestmove "
! echo "$output" | grep -Eq "(Assertion|Segmentation fault|Aborted|Illegal instruction)"

# 2) Original 3-in-a-row removal regression: all tadpoles removed as expected
output=$(run_uci <<'CMDS'
uci
setoption name VariantPath value variants.ini
setoption name UCI_Variant value pond
position startpos moves E@b3 E@d3 E@c3 E@c2
d
quit
CMDS
)

echo "$output" | grep -Fq "Fen: 4/4/2e1/4[EEEEEEEEEEEeeeeeeeeeee] w - - 0 3 {2 1}"

# 3) Simultaneous removeConnectN: two horizontal lines removed in one move
output=$(run_uci <<'CMDS'
uci
setoption name VariantPath value variants.ini
setoption name UCI_Variant value pond
position fen TTT1/4/3T/TTT1 w - - 0 1 {0 0} moves d2d3
d
quit
CMDS
)

echo "$output" | grep -Fq "Fen: 4/3T/4/4[] b - - 1 1 {6 0}"

# 4) Corner-edge diagonal removeConnectN
output=$(run_uci <<'CMDS'
uci
setoption name VariantPath value variants.ini
setoption name UCI_Variant value pond
position fen T3/2T1/1T2/T3 w - - 0 1 {0 0} moves a4b4
d
quit
CMDS
)

echo "$output" | grep -Fq "Fen: 1T2/4/4/4[] b - - 1 1 {3 0}"

# 5) Stalemate terminal check (no legal moves means loss in pond)
output=$(run_uci <<'CMDS'
uci
setoption name VariantPath value variants.ini
setoption name UCI_Variant value pond
position fen 1T2/4/4/4[] b - - 1 1 {3 0}
go depth 2
quit
CMDS
)

echo "$output" | grep -Fq "bestmove (none)"

echo "pond testing OK"
