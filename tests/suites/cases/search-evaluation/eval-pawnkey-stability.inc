#!/usr/bin/env bash
# Pawn-hash entries must not leak king shelter across positions that differ
# only in SHOGI_PAWN/SOLDIER placement (upstream #1044). The key covers
# ordinary pawns; shelter also reads the other two types, so entries must
# not be shared when they are present. This test evaluates B after A in one
# session (shared cache) and B alone in a fresh session, and requires the
# same King safety line both times.

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/../../../../tests/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "eval pawn key stability"

FENA='rnbakabnr/9/1c5c1/p1p1p1p1p/9/9/P1P1P1P1P/1C5C1/9/RNBAKABNR w - - 0 1'
FENB='rnbakabnr/9/1c5c1/p1p1p1p1p/9/9/P1P3P1P/1C5C1/9/RNBAKABNR w - - 0 1'

king_safety_last() {
  grep -i 'king safety' | tail -n 1 | tr -s ' '
}

sequential=$(cat <<CMDS | "${ENGINE}"
uci
setoption name VariantPath value ${VARIANTS}
setoption name UCI_Variant value xiangqi
setoption name Use NNUE value false
position fen ${FENA}
eval
position fen ${FENB}
eval
quit
CMDS
)
if grep -q "unknown variant 'xiangqi'" <<<"${sequential}"; then
  fsx_test_note_skip "xiangqi is unavailable in this engine build"
  exit 0
fi
sequential_b=$(king_safety_last <<<"${sequential}")

fresh=$(cat <<CMDS | "${ENGINE}"
uci
setoption name VariantPath value ${VARIANTS}
setoption name UCI_Variant value xiangqi
setoption name Use NNUE value false
position fen ${FENB}
eval
quit
CMDS
)
fresh_b=$(king_safety_last <<<"${fresh}")

if [[ -z "${sequential_b}" || -z "${fresh_b}" ]]; then
  echo "eval pawn key stability test produced no King safety line" >&2
  exit 1
fi
if [[ "${sequential_b}" != "${fresh_b}" ]]; then
  echo "stale pawn hash entry: sequential '${sequential_b}' vs fresh '${fresh_b}'" >&2
  exit 1
fi

echo "eval pawn key stability tests passed"
