#!/bin/bash

set -euo pipefail

error() {
  echo "hindustani baseline test failed on line $1" >&2
  exit 1
}
trap 'error ${LINENO}' ERR

ROOT=$(cd "$(dirname "$0")/.." && pwd)
ENGINE=${1:-"${ROOT}/src/stockfish"}
VARIANT_PATH=${2:-"${ROOT}/src/variants.ini"}

ROOT="$ROOT" python3 - <<'PY'
import os
import pyffish as sf

cfg = open(os.path.join(os.environ["ROOT"], "src", "variants.ini"), encoding='utf-8').read()
sf.load_variant_config(cfg)

cases = [
    ("7k/8/8/8/8/8/8/7K w - - 0 1", 0),
    ("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1", -sf.VALUE_MATE),
]

for fen, expected in cases:
    result = sf.game_result("hindustani", fen, [])
    if result != expected:
        raise SystemExit(f"unexpected Hindustani result for {fen}: got {result}, expected {expected}")
PY

run_cmds() {
  cat <<EOF | "${ENGINE}" 2>/dev/null
uci
setoption name VariantPath value ${VARIANT_PATH}
setoption name UCI_Variant value hindustani
$1
quit
EOF
}

out=$(run_cmds "position fen 4k3/P7/8/8/8/8/8/4K3 w - - 0 1
go perft 1")
echo "${out}" | grep -q "^a7a8r: 1$"
! echo "${out}" | grep -q "^a7a8q: 1$"

out=$(run_cmds "position fen 4k3/2P5/8/8/8/8/8/4K3 w - - 0 1
go perft 1")
echo "${out}" | grep -q "^c7c8x: 1$"
! echo "${out}" | grep -q "^c7c8b: 1$"

out=$(run_cmds "position fen 4k3/4P3/8/8/8/8/8/4K3 w - - 0 1
go perft 1")
! echo "${out}" | grep -q "^e7e8"

out=$(run_cmds "position fen 4k3/2P5/8/8/8/8/8/2X1K3 w - - 0 1
go perft 1")
! echo "${out}" | grep -q "^c7c8x: 1$"

out=$(run_cmds "position startpos
go perft 1")
echo "${out}" | grep -q "^e1d3: 1$"
echo "${out}" | grep -q "^e1f3: 1$"
