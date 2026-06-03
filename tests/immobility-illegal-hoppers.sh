#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "immobility illegal hoppers regression"

load_inline_variants <<'INI'
[immobility-illegal-hopper-test:chess]
maxFile = h
maxRank = 8
pieceDrops = true
immobilityIllegal = true
king = k:W
customPiece1 = m:fpR
customPiece2 = g:W
promotedPieceType = m:g
startFen = 8/8/8/8/8/8/8/4K3[M]
INI
tmp_ini="${FSX_TMP_INI}"

out=$(run_uci "$ENGINE" "$tmp_ini" immobility-illegal-hopper-test <<'EOF'
position fen 8/8/8/8/8/8/8/4K3[M] w - - 0 1
go perft 1
EOF
)

grep -q '^M@a6:' <<<"$out"
grep -q '^M@e6:' <<<"$out"
! grep -q '^M@a7:' <<<"$out"
! grep -q '^M@e7:' <<<"$out"
! grep -q '^M@a8:' <<<"$out"
! grep -q '^M@e8:' <<<"$out"

echo "immobility-illegal hoppers regression passed"
