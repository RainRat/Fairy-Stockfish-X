#!/bin/bash

set -euo pipefail

error() {
  echo "incomplete baselines test failed on line $1" >&2
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./stockfish}
VARIANT_PATH=${2:-src/variants-incomplete.ini}

if [[ ! -f "${VARIANT_PATH}" && -f "variants-incomplete.ini" ]]; then
  VARIANT_PATH="variants-incomplete.ini"
fi

run_cmds() {
  cat <<EOF | "${ENGINE}" 2>/dev/null
uci
setoption name VariantPath value ${VARIANT_PATH}
$1
quit
EOF
}

# Seega baseline: opening setup excludes the center square.
out=$(run_cmds "setoption name UCI_Variant value seega
position startpos
go perft 1")
! echo "${out}" | grep -q "^D@c3:"

# Seega baseline: custodial capture removes the sandwiched piece.
out=$(run_cmds "setoption name UCI_Variant value seega
position fen d4/5/1D1dD/5/d4 w - - 0 1 moves b3c3
d")
echo "${out}" | grep -Eq "Fen: d4/5/2D1D/5/d4(\\[\\])? b - - 1 1"

# Seega baseline: a blocked side passes rather than losing immediately.
out=$(run_cmds "setoption name UCI_Variant value seega
position fen 5/2D2/1DdD1/D1D2/dD3 b - - 0 1
go perft 1")
echo "${out}" | grep -q "^0000: 1$"

ROOT=$(cd "$(dirname "$0")/.." && pwd)
ROOT="$ROOT" python3 - <<'PY'
import os
import pyffish as sf

cfg = open(os.path.join(os.environ["ROOT"], "src", "variants-incomplete.ini"), encoding="utf-8").read()
sf.load_variant_config(cfg)

fen = "5/5/5/5/1D3[] b - - 0 1"
result = sf.game_result("seega", fen, [])
if result != sf.VALUE_MATE:
    raise SystemExit(f"unexpected Seega extinction result for {fen}: got {result}")
PY

# Oshi baseline: documented 9x9 setup loads with black to move first.
out=$(run_cmds "setoption name UCI_Variant value oshi
position startpos
d")
echo "${out}" | grep -q "Fen: cb1aaa1bc/4a4/9/9/9/9/9/4A4/CB1AAA1BC b - - 0 1 {0 0}"

# Oshi baseline: shoving an enemy height-3 tower off the board gives 3 points to the shover.
out=$(run_cmds "setoption name UCI_Variant value oshi
position fen 9/9/9/9/9/9/9/C8/c8 w - - 0 1 {0 0} moves a2a1
d")
echo "${out}" | grep -q "Fen: 9/9/9/9/9/9/9/9/C8 b - - 0 1 {3 0}"

# Oshi baseline: shoving your own height-3 tower off the board gives 3 points to the opponent.
out=$(run_cmds "setoption name UCI_Variant value oshi
position fen 9/9/9/9/9/9/9/C8/C8 w - - 0 1 {0 0} moves a2a1
d")
echo "${out}" | grep -q "Fen: 9/9/9/9/9/9/9/9/C8 b - - 0 1 {0 3}"

ROOT=$(cd "$(dirname "$0")/.." && pwd)
ROOT="$ROOT" python3 - <<'PY'
import os
import pyffish as sf

cfg = open(os.path.join(os.environ["ROOT"], "src", "variants-incomplete.ini"), encoding="utf-8").read()
sf.load_variant_config(cfg)

if sf.game_result("oshi", "C8/9/9/9/9/9/9/9/9 b - - 0 1 {7 0}", []) != sf.VALUE_MATE:
    raise SystemExit("unexpected Oshi points-goal result")
if sf.game_result("oshi", "9/9/9/9/9/9/9/9/9 b - - 0 1 {8 7}", []) != sf.VALUE_MATE:
    raise SystemExit("unexpected Oshi simultaneous-goal result")
if sf.game_result("oshi", "9/9/9/9/9/9/9/9/9 b - - 0 1 {0 0}", []) != -sf.VALUE_MATE:
    raise SystemExit("unexpected Oshi stalemate result")
PY


ROOT=$(cd "$(dirname "$0")/.." && pwd)
ROOT="$ROOT" python3 - <<'PY'
import os
import pyffish as sf

cfg = open(os.path.join(os.environ["ROOT"], "src", "variants-incomplete.ini"), encoding="utf-8").read()
sf.load_variant_config(cfg)
PY
