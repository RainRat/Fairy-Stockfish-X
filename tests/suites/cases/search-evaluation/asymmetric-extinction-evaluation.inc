#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/../../../../tests/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "asymmetric extinction evaluation"

load_inline_variants <<'INI'
[asym-extinction-evaluation:chess]
checking = false
castling = false
extinctionValueWhite = win
extinctionPieceTypesWhite = q
extinctionAllPieceTypesWhite = false
extinctionValueBlack = loss
extinctionPieceTypesBlack = p
extinctionAllPieceTypesBlack = false
INI

extract_eval() {
    sed -n 's/^Final evaluation[[:space:]]*//p' | tail -n1 | awk '{print $1}'
}

fen_for_side() {
    printf '4k3/8/8/3p4/3Q4/8/8/4K3 %s - - 0 1' "$1"
}

white_eval=$(run_uci "${ENGINE}" "${FSX_TMP_INI}" asym-extinction-evaluation <<UCI | extract_eval
setoption name Use NNUE value false
position fen $(fen_for_side w)
eval
UCI
)
black_eval=$(run_uci "${ENGINE}" "${FSX_TMP_INI}" asym-extinction-evaluation <<UCI | extract_eval
setoption name Use NNUE value false
position fen $(fen_for_side b)
eval
UCI
)

[[ -n "${white_eval}" && "${white_eval}" == "${black_eval}" ]]

echo "asymmetric extinction evaluation regression passed"
