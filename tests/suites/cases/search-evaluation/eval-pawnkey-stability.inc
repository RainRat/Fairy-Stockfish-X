#!/usr/bin/env bash
# Pawn-hash entries must not leak unevaluated state across positions sharing
# the pawn key (upstream #1044). The key covers ordinary pawns, but king
# shelter also reads SHOGI_PAWN/SOLDIER and the cached score doubles when a
# side has only pawns, so the probe key must include those pieces' placement
# and a per-color bare flag. This test evaluates B after A in one session
# (shared cache) and B alone in a fresh session, and requires the same
# evaluated row both times.

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/../../../../tests/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "eval pawn key stability"

FENA='rnbakabnr/9/1c5c1/p1p1p1p1p/9/9/P1P1P1P1P/1C5C1/9/RNBAKABNR w - - 0 1'
FENB='rnbakabnr/9/1c5c1/p1p1p1p1p/9/9/P1P3P1P/1C5C1/9/RNBAKABNR w - - 0 1'
# Soldier disappears entirely in B: entries computed with soldiers present
# must not be reused by soldier-free positions sharing the pawn key.
FENC='rnbkaabnr/9/1c5c1/4P4/9/9/9/1C5C1/9/RNBAKABNR w - - 0 1'
FEND='rnbkaabnr/9/1c5c1/9/9/9/9/1C5C1/9/RNBAKABNR w - - 0 1'

check_pair() {
  local first="$1" second="$2" label="$3" variant="${4:-xiangqi}" row_pat="${5:-king safety}"
  local sequential fresh sequential_b fresh_b
  sequential=$(cat <<CMDS | "${ENGINE}"
uci
setoption name VariantPath value ${VARIANTS}
setoption name UCI_Variant value ${variant}
setoption name Use NNUE value false
position fen ${first}
eval
position fen ${second}
eval
quit
CMDS
)
  if grep -q "unknown variant '${variant}'" <<<"${sequential}"; then
    fsx_test_note_skip "${variant} is unavailable in this engine build"
    exit 0
  fi
  sequential_b=$(grep -i "${row_pat}" <<<"${sequential}" | tail -n 1 | tr -s ' ')

  fresh=$(cat <<CMDS | "${ENGINE}"
uci
setoption name VariantPath value ${VARIANTS}
setoption name UCI_Variant value ${variant}
setoption name Use NNUE value false
position fen ${second}
eval
quit
CMDS
)
  fresh_b=$(grep -i "${row_pat}" <<<"${fresh}" | tail -n 1 | tr -s ' ')

  if [[ -z "${sequential_b}" || -z "${fresh_b}" ]]; then
    echo "eval pawn key stability test produced no '${row_pat}' line (${label})" >&2
    exit 1
  fi
  if [[ "${sequential_b}" != "${fresh_b}" ]]; then
    echo "stale pawn hash entry (${label}): sequential '${sequential_b}' vs fresh '${fresh_b}'" >&2
    exit 1
  fi
}

check_pair "${FENA}" "${FENB}" "soldier-placement"
check_pair "${FENC}" "${FEND}" "soldier-disappears"

# The cached "pawns only" doubling depends on non-pawn absence, which the
# pawn key also omits. White drops from pawns+knight to pawns only with an
# identical pawn skeleton; the bare position must not reuse the undoubled
# entry (horde white has no king, so the bare condition is reachable).
check_pair 'r3k3/8/8/8/4N3/8/PPPPPPPP/8 w - - 0 1' \
          'r3k3/8/8/8/8/8/PPPPPPPP/8 w - - 0 1' \
          "pawns-only-doubling" "horde" "| *pawns |"

echo "eval pawn key stability tests passed"
