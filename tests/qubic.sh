#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
ENGINE="${1:-${REPO_ROOT}/src/stockfish}"
VARIANTS=${2:-src/${REPO_ROOT}/src/variants.ini}

run_cmds() {
  printf 'uci\nsetoption name VariantPath value %s\n%s\nquit\n' "$VARIANTS" "$1" | "$ENGINE"
}

variant_available() {
  local out
  out=$(run_cmds "setoption name UCI_Variant value qubic
d")
  echo "$out" | grep -q "info string variant qubic "
}

echo "qubic regression tests started"

if ! variant_available; then
  echo "qubic regression skipped: variant unavailable in this build"
  exit 0
fi

out=$(run_cmds "setoption name UCI_Variant value qubic
position fen 8/8/8/8/8/8/8/8[pppppppppppppppppppppppppppppppp] b - - 0 1
go perft 1")
echo "$out" | grep -q "Nodes searched: 64"

out=$(run_cmds "setoption name UCI_Variant value qubic
position fen 8/8/8/P3P3/8/8/8/P3P3[pppppppppppppppppppppppppppp] b - - 0 1
go perft 1")
echo "$out" | grep -q "Nodes searched: 0"

out=$(run_cmds "setoption name UCI_Variant value qubic
position fen 8/8/8/8/8/8/8/P7[ppppppppppppppppppppppppppppppp] b - - 0 1
go perft 1")
echo "$out" | grep -q "Nodes searched: 63"

out=$(run_cmds "setoption name UCI_Variant value qubic
position fen 7P/2P5/8/8/8/8/5P2/P7[pppppppppppppppppppppppppppp] b - - 0 1
go perft 1")
echo "$out" | grep -q "Nodes searched: 0"

echo "qubic regression tests passed"