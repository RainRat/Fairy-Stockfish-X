#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/../../../../tests/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "nnue variant dimension guard"
if [[ ! -x "${ENGINE}" ]]; then
  if [[ "${FSX_ALLOW_SMALL_BOARD:-0}" == 1 ]]; then
    fsx_test_skip "VLB engine for NNUE dimension guard is unavailable at ${ENGINE}; skip allowed by FSX_ALLOW_SMALL_BOARD=1"
  fi
  echo "NNUE dimension guard requires a VLB engine: ${ENGINE}" >&2
  exit 1
fi

load_inline_variants <<'EOF'
[nnguard:fairy]
maxFile = 12
maxRank = 10
pieceDrops = true
captureType = hand
pocketSize = 12
customPiece1 = a:W
customPiece2 = c:F
customPiece3 = d:N
customPiece4 = e:B
customPiece5 = f:R
customPiece6 = g:Q
customPiece7 = h:K
customPiece8 = i:A
startFen = 11k/12/12/12/12/12/12/12/12/11K[] w - - 0 1
EOF
tmp_ini="${FSX_TMP_INI}"

out=$(run_uci "$ENGINE" "$tmp_ini" nnguard <<'EOF'
setoption name EvalFile value nnguard.nnue
position startpos
go depth 1
EOF
)

assert_contains "$out" "info string NNUE disabled for variant nnguard"
assert_contains "$out" "info string classical evaluation enabled"
assert_not_contains "$out" "The option is set to true, but the network file"

echo "nnue variant dimension guard passed"
