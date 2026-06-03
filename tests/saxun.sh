#!/usr/bin/env bash
set -euo pipefail

ENGINE=${1:-./stockfish}
VARIANTS=${2:-src/variants.ini}

run_cmds() {
  printf 'uci\nsetoption name VariantPath value %s\n%s\nquit\n' "$VARIANTS" "$1" | "$ENGINE"
}

echo "saxun regression tests started"

out=$(run_cmds "setoption name UCI_Variant value saxun
position startpos
d")
echo "$out" | grep -Fq "Fen: 1rk4n/nqr2bb1/4pppp/ppp5/5PPP/1PPPP3/P1RQ3N/1NKRBB2 w - - 0 1"

out=$(run_cmds "setoption name UCI_Variant value saxun
position fen 8/8/8/8/8/8/P7/8 w - - 0 1
go perft 1")
echo "$out" | grep -Fq "a2a3: 1"
! echo "$out" | grep -Fq "a2a4:"

out=$(run_cmds "setoption name UCI_Variant value saxun
position fen 8/P7/8/8/8/8/8/8 w - - 0 1
go perft 1")
! echo "$out" | grep -Eq '^a7a8[a-z]: 1$'
! echo "$out" | grep -Fq "a7a8:"

echo "saxun regression tests passed"
