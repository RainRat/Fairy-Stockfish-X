#!/usr/bin/env bash
set -euo pipefail

ENGINE=${1:-./stockfish}
VARIANTS=${2:-src/variants.ini}

run_cmds() {
  printf 'uci\nsetoption name VariantPath value %s\n%s\nquit\n' "$VARIANTS" "$1" | "$ENGINE"
}

echo "villagers regression tests started"

out=$(run_cmds "setoption name UCI_Variant value villagers
d")
echo "$out" | grep -q "info string variant villagers "

# Royal pawn can move one or two squares from the back rank when unobstructed.
out=$(run_cmds "setoption name UCI_Variant value villagers
position fen 7u/8/8/8/8/8/8/3U4 w - - 0 1
go perft 1")
echo "$out" | grep -q "^d1d2: 1$"
echo "$out" | grep -q "^d1d3: 1$"

# Soldiers can move one or two squares from the second rank.
out=$(run_cmds "setoption name UCI_Variant value villagers
position fen 7u/8/8/8/8/8/3S4/7U w - - 0 1
go perft 1")
echo "$out" | grep -q "^d2d3: 1$"
echo "$out" | grep -q "^d2d4: 1$"

# Sergeants may capture en passant.
out=$(run_cmds "setoption name UCI_Variant value villagers
position fen 7u/8/8/3sG3/8/8/8/U7 w - d6 0 1
go perft 1")
echo "$out" | grep -q "^e5d6: 1$"

# Royal pawns promote while keeping royal status through dedicated royal piece types.
out=$(run_cmds "setoption name UCI_Variant value villagers
position fen 8/3U4/8/8/8/8/8/7u w - - 0 1
go perft 1")
echo "$out" | grep -q "^d7d8a: 1$"
echo "$out" | grep -q "^d7d8c: 1$"
echo "$out" | grep -q "^d7d8m: 1$"
echo "$out" | grep -q "^d7d8l: 1$"

# Capturing the royal pawn en passant must end the game instead of continuing.
out=$(run_cmds "setoption name UCI_Variant value villagers
setoption name Verbosity value 2
position fen 8/8/8/3uU3/8/8/8/8 w - d6 0 1 moves e5d6
go depth 1")
echo "$out" | grep -q "adjudication reason game_end"
echo "$out" | grep -q "^bestmove (none)$"

echo "villagers regression tests passed"
