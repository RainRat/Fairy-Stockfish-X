#!/bin/bash
# Extinction all-types regression tests

set -euo pipefail

error() {
  echo "extinction-all-types testing failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./stockfish}

echo "extinction-all-types testing started"

cfg=$(mktemp)
check_out=$(mktemp)
cleanup() {
  rm -f "$cfg" "$check_out"
}
trap cleanup EXIT

run_uci() {
  local cmd_file
  cmd_file=$(mktemp)
  cat > "$cmd_file"
  local local_out
  local_out=$(mktemp)
  timeout 20s "$ENGINE" < "$cmd_file" > "$local_out" 2>&1
  rm -f "$cmd_file"
  cat "$local_out"
  rm -f "$local_out"
}

cat > "$cfg" <<'EOF'
[extinct-any:chess]
extinctionValue = loss
extinctionPieceTypes = qr
checking = false
castling = false

[extinct-all:extinct-any]
extinctionAllPieceTypes = true
EOF

"$ENGINE" check "$cfg" > "$check_out" 2>&1

out=$(run_uci <<CMDS
uci
setoption name VariantPath value $cfg
setoption name UCI_Variant value extinct-any
position fen 3qk2r/8/8/8/8/8/4R3/4K3 w - - 0 1
go depth 1
quit
CMDS
)
echo "$out" | grep -Fq "info string variant extinct-any"
echo "$out" | grep -Fq "bestmove (none)"

out=$(run_uci <<CMDS
uci
setoption name VariantPath value $cfg
setoption name UCI_Variant value extinct-all
position fen 3qk2r/8/8/8/8/8/4R3/4K3 w - - 0 1
go depth 1
quit
CMDS
)
echo "$out" | grep -Fq "info string variant extinct-all"
echo "$out" | grep -Fq "bestmove e2a2"

out=$(run_uci <<CMDS
uci
setoption name VariantPath value $cfg
setoption name UCI_Variant value extinct-all
position fen 3qk2r/8/8/8/8/8/8/4K3 w - - 0 1
go depth 1
quit
CMDS
)
echo "$out" | grep -Fq "bestmove (none)"

echo "extinction-all-types testing OK"
