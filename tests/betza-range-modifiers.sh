#!/bin/bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

ENGINE=$(default_engine "${1:-}")

TMP_VARIANT_PATH=$(mktemp "${TMPDIR:-/tmp}/fsx-betza-range-XXXXXX.ini")
trap 'rm -f "${TMP_VARIANT_PATH}"' EXIT

cat >"${TMP_VARIANT_PATH}" <<'INI'
[range35:chess]
king = -
checking = false
customPiece1 = a:R[3-5]
pieceToCharTable = A:a
startFen = 8/8/8/8/4A3/8/8/8 w - - 0 1

[range3plus:chess]
king = -
checking = false
customPiece1 = a:R[3-]
pieceToCharTable = A:a
startFen = 8/8/8/8/4A3/8/8/8 w - - 0 1

[rangeinvalid:chess]
king = -
checking = false
customPiece1 = a:R[3]
pieceToCharTable = A:a
startFen = 8/8/8/8/4A3/8/8/8 w - - 0 1
INI

echo "betza range modifiers tests started"

out=$(run_uci "$ENGINE" "$TMP_VARIANT_PATH" range35 <<'UCI'
position startpos
go perft 1
UCI
)
assert_contains "$out" "^e4e7: 1$"
assert_contains "$out" "^e4e8: 1$"
assert_contains "$out" "^e4b4: 1$"
assert_contains "$out" "^e4h4: 1$"
assert_not_contains "$out" "^e4e5: 1$"
assert_not_contains "$out" "^e4e6: 1$"
assert_not_contains "$out" "^e4d4: 1$"
assert_not_contains "$out" "^e4c4: 1$"

out=$(run_uci "$ENGINE" "$TMP_VARIANT_PATH" range3plus <<'UCI'
position startpos
go perft 1
UCI
)
assert_contains "$out" "^e4e7: 1$"
assert_contains "$out" "^e4e8: 1$"
assert_contains "$out" "^e4b4: 1$"
assert_contains "$out" "^e4h4: 1$"
assert_not_contains "$out" "^e4e5: 1$"
assert_not_contains "$out" "^e4e6: 1$"
assert_not_contains "$out" "^e4d4: 1$"
assert_not_contains "$out" "^e4c4: 1$"

invalid_out=$(run_uci "$ENGINE" "$TMP_VARIANT_PATH" rangeinvalid <<'UCI' 2>&1
UCI
)
assert_contains "$invalid_out" "Invalid Betza rider range"

echo "betza range modifiers tests passed"
