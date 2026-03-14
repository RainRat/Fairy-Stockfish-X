#!/usr/bin/env bash
set -euo pipefail

engine=${1:-src/stockfish}

uci_output=$(
  printf 'uci\nquit\n' | "$engine"
)
grep -q 'option name Verbosity type spin default 1 min 0 max 2' <<<"$uci_output"

quiet_output=$(
  printf 'uci\nsetoption name Verbosity value 0\nposition startpos\ngo depth 2\nquit\n' | "$engine"
)
if grep -q '^info depth ' <<<"$quiet_output"; then
  echo "Verbosity=0 unexpectedly emitted search info"
  exit 1
fi

debug_output=$(
  printf 'uci\nsetoption name Verbosity value 2\nposition fen 7k/5Q2/7K/8/8/8/8/8 b - - 0 1\ngo depth 1\nquit\n' | "$engine"
)
grep -q 'info string adjudication reason stalemate result cp 0 side_to_move black' <<<"$debug_output"

echo "verbosity regression passed"
