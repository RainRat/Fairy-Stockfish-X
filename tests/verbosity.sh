#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ENGINE="${1:-${SCRIPT_DIR}/../src/stockfish}"
source "${SCRIPT_DIR}/lib/uci.sh"

uci_output=$(
  printf 'uci\nquit\n' | uci_timeout "$ENGINE"
)
assert_contains "$uci_output" 'option name Verbosity type spin default 1 min 0 max 2'

quiet_output=$(
  printf 'uci\nsetoption name Verbosity value 0\nposition startpos\ngo depth 2\nquit\n' | uci_timeout "$ENGINE"
)
if grep -q '^info depth ' <<<"$quiet_output"; then
  echo "Verbosity=0 unexpectedly emitted search info"
  exit 1
fi

debug_output=$(
  printf 'uci\nsetoption name Verbosity value 2\nposition fen 7k/5Q2/7K/8/8/8/8/8 b - - 0 1\ngo depth 1\nquit\n' | uci_timeout "$ENGINE"
)
assert_contains "$debug_output" 'info string adjudication reason stalemate result cp 0 side_to_move black'

echo "verbosity regression passed"
