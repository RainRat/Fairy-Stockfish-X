#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ENGINE="${1:-${SCRIPT_DIR}/../src/stockfish}"
VARIANTS="${2:-${SCRIPT_DIR}/../src/variants.ini}"
source "${SCRIPT_DIR}/lib/uci.sh"

run_cmds() {
  run_uci "$ENGINE" "$VARIANTS" separate-realms <<EOF
$1
EOF
}

variant_available() {
  local out
  out=$(printf 'uci\nquit\n' | uci_timeout "$ENGINE")
  grep -q ' var separate-realms ' <<<"$out"
}

echo "separate realms regression tests started"

if ! variant_available; then
  echo "separate-realms variant not available in this build; skipping separate-realms regression"
  exit 0
fi

out=$(run_cmds "setoption name UCI_Variant value separate-realms
position startpos
go perft 1")
assert_contains "$out" "^Nodes searched: 36$"
assert_contains "$out" "^b1a3: 1$"
assert_not_contains "$out" "^b1d2: 1$"

out=$(run_cmds "setoption name UCI_Variant value separate-realms
position fen 4k3/8/8/8/3K4/8/8/8 w - - 0 1
go perft 1")
assert_contains "$out" "^d4c5: 1$"
assert_contains "$out" "^d4e5: 1$"
assert_not_contains "$out" "^d4d5: 1$"

out=$(run_cmds "setoption name UCI_Variant value separate-realms
position fen 4k3/8/8/3r4/3K4/8/8/8 w - - 0 1
go perft 1")
assert_contains "$out" "^d4d5: 1$"

out=$(run_cmds "setoption name UCI_Variant value separate-realms
position fen 4k3/8/8/8/3C4/8/8/4K3 w - - 0 1
go perft 1")
assert_contains "$out" "^d4f6: 1$"
assert_contains "$out" "^d4h8: 1$"
assert_contains "$out" "^d4b6: 1$"
assert_contains "$out" "^d4f2: 1$"
assert_contains "$out" "^d4b2: 1$"
assert_not_contains "$out" "^d4e5: 1$"

out=$(run_cmds "setoption name UCI_Variant value separate-realms
position fen 4k3/8/8/4r3/3C4/8/8/4K3 w - - 0 1
go perft 1")
assert_contains "$out" "^d4e5: 1$"
assert_contains "$out" "^d4f6: 1$"

out=$(run_cmds "setoption name UCI_Variant value separate-realms
position fen 4k3/8/8/8/3E4/8/8/4K3 w - - 0 1
go perft 1")
assert_contains "$out" "^d4d6: 1$"
assert_contains "$out" "^d4d8: 1$"
assert_contains "$out" "^d4f4: 1$"
assert_contains "$out" "^d4h4: 1$"
assert_contains "$out" "^d4d2: 1$"
assert_contains "$out" "^d4b4: 1$"
assert_not_contains "$out" "^d4d5: 1$"

out=$(run_cmds "setoption name UCI_Variant value separate-realms
position fen 4k3/8/8/3r4/3E4/8/8/4K3 w - - 0 1
go perft 1")
assert_contains "$out" "^d4d5: 1$"
assert_contains "$out" "^d4d6: 1$"

out=$(run_cmds "setoption name UCI_Variant value separate-realms
position fen 4k3/8/8/8/3A4/8/8/4K3 w - - 0 1
go perft 1")
assert_contains "$out" "^d4c6: 1$"
assert_contains "$out" "^d4e6: 1$"
assert_contains "$out" "^d4c2: 1$"
assert_contains "$out" "^d4e2: 1$"
assert_not_contains "$out" "^d4f5: 1$"
assert_not_contains "$out" "^d4b5: 1$"

out=$(run_cmds "setoption name UCI_Variant value separate-realms
position fen 4k3/8/8/5r2/3A4/8/8/4K3 w - - 0 1
go perft 1")
assert_contains "$out" "^d4f5: 1$"

echo "separate realms regression tests passed"
