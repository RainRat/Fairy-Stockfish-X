#!/bin/bash

set -euo pipefail

error() {
  echo "potion custom test failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./stockfish}
DEFAULT_VARIANT_PATH="variants.ini"
if [[ ! -f "${DEFAULT_VARIANT_PATH}" && -f "src/variants.ini" ]]; then
  DEFAULT_VARIANT_PATH="src/variants.ini"
fi
VARIANT_PATH=${2:-${DEFAULT_VARIANT_PATH}}

run_cmds() {
  cat <<CMDS | "${ENGINE}"
uci
setoption name VariantPath value ${VARIANT_PATH}
$1
quit
CMDS
}

echo "potion custom tests started"

# Issue 2: Jump potion with two blockers where removing both would make the move possible,
# but removing only the encoded gate square does not.
out=$(run_cmds "setoption name UCI_Variant value spell-chess
position fen 7k/8/8/p7/8/p7/8/R3K3[J] w - - 0 1
go perft 1")

# j@a3,a1a4 is legal (only a3 blocks)
echo "${out}" | grep -q "^j@a3,a1a4: 1$"
# j@a3,a1a5 is legal (a5 is capture destination, only a3 blocks)
echo "${out}" | grep -q "^j@a3,a1a5: 1$"
# j@a3,a1a6 is illegal (a5 is on the path and not removed)
! echo "${out}" | grep -q "^j@a3,a1a6:"

# Issue 4: Verify that a checkers king (where allow_checks() is false and there is no royal)
# does not trigger the fallback when missing.
# In checkers, startpos has no kings, but king_type() is KING.
# If the fallback triggered, checkers startpos would think the king is missing
# and immediately declare checkmate/end of game (0 perft nodes).
# Let's verify that checkers has valid moves from startpos.
out=$(run_cmds "setoption name UCI_Variant value checkers
position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched: 7"

# Malformed cooldown syntax must not consume the remainder as valid cooldowns
# or retain previously parsed cooldown state.
out=$(run_cmds "setoption name UCI_Variant value spell-chess
position fen 7k/8/8/8/8/8/8/4K3[J] w - - 0 1 - <1 2 3 4>
d
position fen 7k/8/8/8/8/8/8/4K3[J] w - - 0 1 - <1 2 3 4
d
position fen 7k/8/8/8/8/8/8/4K3[J] w - - 0 1 - <1 2 x 4>
d" 2>&1)
grep -q "^Fen: 7k/8/8/8/8/8/8/4K3\\[J\\] w - - 0 1 - <1 2 3 4>$" <<<"${out}"
grep -q "^Fen: 7k/8/8/8/8/8/8/4K3\\[J\\] w - - 0 1$" <<<"${out}"
grep -q "^Fen: 7k/8/8/8/8/8/8/4K3\\[J\\] w - - 0 1$" <<<"${out}"
grep -q "^Invalid potion cooldown specification in FEN: '<1 2 x 4>'\.$" <<<"${out}"

echo "potion custom tests passed"
