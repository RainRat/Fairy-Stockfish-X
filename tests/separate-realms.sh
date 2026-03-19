#!/usr/bin/env bash
set -euo pipefail

ENGINE=${1:-./stockfish}
VARIANTS=${2:-src/variants.ini}

run_cmds() {
  printf 'uci\nsetoption name VariantPath value %s\n%s\nquit\n' "$VARIANTS" "$1" | "$ENGINE"
}

echo "separate realms regression tests started"

out=$(run_cmds "setoption name UCI_Variant value separate-realms
position startpos
go perft 1")
echo "$out" | grep -q "Nodes searched: 36"
echo "$out" | grep -q "^b1a3: 1$"
! echo "$out" | grep -q "^b1d2: 1$"

out=$(run_cmds "setoption name UCI_Variant value separate-realms
position fen 4k3/8/8/8/3K4/8/8/8 w - - 0 1
go perft 1")
echo "$out" | grep -q "^d4c5: 1$"
echo "$out" | grep -q "^d4e5: 1$"
! echo "$out" | grep -q "^d4d5: 1$"

out=$(run_cmds "setoption name UCI_Variant value separate-realms
position fen 4k3/8/8/3r4/3K4/8/8/8 w - - 0 1
go perft 1")
echo "$out" | grep -q "^d4d5: 1$"

out=$(run_cmds "setoption name UCI_Variant value separate-realms
position fen 4k3/8/8/8/3C4/8/8/4K3 w - - 0 1
go perft 1")
echo "$out" | grep -q "^d4f6: 1$"
echo "$out" | grep -q "^d4h8: 1$"
echo "$out" | grep -q "^d4b6: 1$"
echo "$out" | grep -q "^d4f2: 1$"
echo "$out" | grep -q "^d4b2: 1$"
! echo "$out" | grep -q "^d4e5: 1$"

out=$(run_cmds "setoption name UCI_Variant value separate-realms
position fen 4k3/8/8/4r3/3C4/8/8/4K3 w - - 0 1
go perft 1")
echo "$out" | grep -q "^d4e5: 1$"
echo "$out" | grep -q "^d4f6: 1$"

out=$(run_cmds "setoption name UCI_Variant value separate-realms
position fen 4k3/8/8/8/3E4/8/8/4K3 w - - 0 1
go perft 1")
echo "$out" | grep -q "^d4d6: 1$"
echo "$out" | grep -q "^d4d8: 1$"
echo "$out" | grep -q "^d4f4: 1$"
echo "$out" | grep -q "^d4h4: 1$"
echo "$out" | grep -q "^d4d2: 1$"
echo "$out" | grep -q "^d4b4: 1$"
! echo "$out" | grep -q "^d4d5: 1$"

out=$(run_cmds "setoption name UCI_Variant value separate-realms
position fen 4k3/8/8/3r4/3E4/8/8/4K3 w - - 0 1
go perft 1")
echo "$out" | grep -q "^d4d5: 1$"
echo "$out" | grep -q "^d4d6: 1$"

out=$(run_cmds "setoption name UCI_Variant value separate-realms
position fen 4k3/8/8/8/3A4/8/8/4K3 w - - 0 1
go perft 1")
echo "$out" | grep -q "^d4c6: 1$"
echo "$out" | grep -q "^d4e6: 1$"
echo "$out" | grep -q "^d4c2: 1$"
echo "$out" | grep -q "^d4e2: 1$"
! echo "$out" | grep -q "^d4f5: 1$"
! echo "$out" | grep -q "^d4b5: 1$"

out=$(run_cmds "setoption name UCI_Variant value separate-realms
position fen 4k3/8/8/5r2/3A4/8/8/4K3 w - - 0 1
go perft 1")
echo "$out" | grep -q "^d4f5: 1$"

echo "separate realms regression tests passed"
