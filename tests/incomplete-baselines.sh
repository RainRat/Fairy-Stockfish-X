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

# Ko-app-paw-na baseline: hunter can hop-capture over one adjacent rabbit.
out=$(run_cmds "setoption name UCI_Variant value ko-app-paw-na
position fen 5/2R2/2h2/5/5 b - - 0 1 moves c3c5
d")
echo "${out}" | grep -q "Fen: 2h2/5/5/5/5 w - - 0 2"

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

# Aries baseline: documented setup loads.
out=$(run_cmds "setoption name UCI_Variant value aries
position startpos
d")
echo "${out}" | grep -q "Fen: 4rrrr/4rrrr/4rrrr/4rrrr/RRRR4/RRRR4/RRRR4/RRRR4 w - - 0 1"

# Aries baseline: pushing an enemy into a friendly blocker captures only the enemy.
out=$(run_cmds "setoption name UCI_Variant value aries
position fen 8/8/8/8/8/8/8/RrR5 w - - 0 1 moves a1b1
d")
echo "${out}" | grep -q "Fen: 8/8/8/8/8/8/8/1RR5 b - - 0 1"

# Aries baseline: edge shove captures the last enemy piece.
out=$(run_cmds "setoption name UCI_Variant value aries
position fen 8/8/8/8/8/8/8/6Rr w - - 0 1 moves g1h1
d")
echo "${out}" | grep -q "Fen: 8/8/8/8/8/8/8/7R b - - 0 1"

ROOT=$(cd "$(dirname "$0")/.." && pwd)
ROOT="$ROOT" python3 - <<'PY'
import os
import pyffish as sf

cfg = open(os.path.join(os.environ["ROOT"], "src", "variants-incomplete.ini"), encoding="utf-8").read()
sf.load_variant_config(cfg)

if sf.game_result("aries", "7R/8/8/8/8/8/8/8 w - - 0 1", []) != sf.VALUE_MATE:
    raise SystemExit("unexpected Aries flag result")
if sf.game_result("aries", "8/8/8/8/8/8/8/7r w - - 0 1", []) != -sf.VALUE_MATE:
    raise SystemExit("unexpected Aries extinction result")
PY

# Aries baseline: search avoids a repetition-losing move when a non-losing move exists.
out=$(run_cmds "setoption name UCI_Variant value aries
position fen 8/8/8/8/8/8/7r/R7 w - - 0 1 moves a1a2 h2h1 a2a1 h1h2 a1a2 h2h1 a2a1
go depth 3")
! echo "${out}" | grep -q "^bestmove h1h2$"

# Control: the repetition-losing move is still legal and searchable when forced.
out=$(run_cmds "setoption name UCI_Variant value aries
position fen 8/8/8/8/8/8/7r/R7 w - - 0 1 moves a1a2 h2h1 a2a1 h1h2 a1a2 h2h1 a2a1
go depth 2 searchmoves h1h2")
echo "${out}" | grep -q "^bestmove h1h2$"
