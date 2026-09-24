#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/../../../../tests/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "Chu Shogi Lion rules"

load_inline_variants <<'INI'
[chu-test:chess]
maxFile = h
customPiece1 = l:KAD
king = -
commoner = k
checking = false
castling = false
twoStepMoves = l:*
lionMoveTypes = l
lionCapturingRule = true
insignificantPieces = p
INI

out=$(run_uci "${ENGINE}" "${FSX_TMP_INI}" chu-test <<'UCI'
position fen 8/8/8/8/2r5/2l5/1p6/L7 w - - 0 1
go perft 1
UCI
)
assert_not_contains "${out}" '^a1b2c3: 1$'

# Capturing a significant piece on the midpoint permits the protected Lion capture.
out=$(run_uci "${ENGINE}" "${FSX_TMP_INI}" chu-test <<'UCI'
position fen 8/8/8/8/2r5/2l5/1r6/L7 w - - 0 1
go perft 1
UCI
)
assert_contains "${out}" '^a1b2c3: 1$'

# Capturing an insignificant pawn on the midpoint does not permit it.
out=$(run_uci "${ENGINE}" "${FSX_TMP_INI}" chu-test <<'UCI'
position fen 8/8/8/8/2r5/2l5/1p6/L7 w - - 0 1
go perft 1
UCI
)
assert_not_contains "${out}" '^a1b2c3: 1$'

# A non-Lion cannot immediately trade a Lion after a non-Lion took its Lion.
out=$(run_uci "${ENGINE}" "${FSX_TMP_INI}" chu-test <<'UCI'
position fen 8/8/8/8/4l3/8/1L6/1r2R3 b - - 0 1 moves b1b2
go perft 1
UCI
)
assert_not_contains "${out}" '^e1e4: 1$'

# The same recapture is allowed when the prior capturer was a Lion.
out=$(run_uci "${ENGINE}" "${FSX_TMP_INI}" chu-test <<'UCI'
position fen 8/8/8/8/1R6/8/1L6/1l6 b - - 0 1 moves b1b2
go perft 1
UCI
)
assert_contains "${out}" '^b4b2: 1$'
