#!/bin/bash

set -euo pipefail

error() {
  echo "largeboard seirawan regression failed on line $1"
  [[ -n "${TMP_VARIANT_PATH:-}" ]] && rm -f "${TMP_VARIANT_PATH}"
  exit 1
}
trap 'error ${LINENO}' ERR

ROOT_DIR=$(cd "$(dirname "$0")/../../../.." && pwd)
source "${ROOT_DIR}/tests/lib/uci.sh"
setup_test_context "${1:-}" "${2:-}" "large-board Seirawan regression"

TMP_VARIANT_PATH=$(mktemp "${TMPDIR:-/tmp}/fsx-seirawan10-XXXXXX")
cat >"${TMP_VARIANT_PATH}" <<'INI'
[seirawan10:chess]
gating = true
seirawanGating = true
maxRank = 10
maxFile = 10
customPiece1 = h:N
pieceToCharTable = H:h
startFen = rnbqkbnr2/pppppppppp/10/10/10/10/10/10/PPPPPPPPPP/RNBQKBNR1R[Hh] w KQ|1000100001/0000000000 - 0 1
INI

out=$(run_uci "$ENGINE" "$TMP_VARIANT_PATH" seirawan10 <<'UCI'
position startpos
go perft 1
UCI
)
assert_contains_literal "$out" "j1i1h: 1" "large-board gating move"
assert_contains_literal "$out" "a2a3h: 1" "large-board pawn gating move"

out=$(run_uci "$ENGINE" "$TMP_VARIANT_PATH" seirawan10 <<'UCI'
position startpos moves j1i1h
d
UCI
)
assert_contains_literal "$out" "[h]" "large-board FEN pocket after gating"
assert_contains_literal "$out" "|0000000000/0000000000" "large-board gating mask round-trip"

rm -f "${TMP_VARIANT_PATH}"
unset TMP_VARIANT_PATH

echo "largeboard seirawan regression passed"
