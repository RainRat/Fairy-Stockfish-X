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
lionInsignificantPieces = p

[chu-okazaki-test:chu-test]
lionOkazakiRule = true

[chu-promo-test:chu-test]
customPiece2 = a:KAD
promotedPieceType = a:l
promotionRegionWhite = *1 *2 *3

[chu-multi-test:chu-test]
customPiece2 = h:K
twoStepMoves = l:* h:N>N

# Cub mover (king steps only, no 2-square leaps) so the straight double
# capture cannot hide behind the direct-leap dedup or the quiet-via route
# dedup: the via pawn capture gives b3c3d3 its own route bucket.
[chu-xray-test:chu-test]
customPiece2 = m:K
twoStepMoves = l:* m:*
lionMoveTypes = l m
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

# Okazaki exception: the same counter-strike is allowed when the target is unprotected.
out=$(run_uci "${ENGINE}" "${FSX_TMP_INI}" chu-okazaki-test <<'UCI'
position fen 8/8/8/8/4l3/8/1L6/1r2R3 b - - 0 1 moves b1b2
go perft 1
UCI
)
assert_contains "${out}" '^e1e4: 1$'

# Okazaki does not allow retaliation against a protected Lion.
out=$(run_uci "${ENGINE}" "${FSX_TMP_INI}" chu-okazaki-test <<'UCI'
position fen 8/8/8/4r3/4l3/8/1L6/1r2R3 b - - 0 1 moves b1b2
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

# A Kirin that promotes after capturing a Lion may be recaptured on that square.
out=$(run_uci "${ENGINE}" "${FSX_TMP_INI}" chu-promo-test <<'UCI'
position fen 8/8/8/8/2r5/2l5/2A5/8 w - - 0 1 moves c2c3+
go perft 1
UCI
)
assert_contains "${out}" '^c4c3: 1$'

# That promotion does not let a non-Lion take a different Lion immediately.
out=$(run_uci "${ENGINE}" "${FSX_TMP_INI}" chu-promo-test <<'UCI'
position fen 8/8/8/8/2r2r2/2l2L2/2A5/8 w - - 0 1 moves c2c3+
go perft 1
UCI
)
assert_not_contains "${out}" '^f4f3: 1$'

# A hit-and-run Lion capture on the via square still triggers the immediate-trade rule.
out=$(run_uci "${ENGINE}" "${FSX_TMP_INI}" chu-multi-test <<'UCI'
position fen r7/L7/8/8/8/4p3/3l4/2H5 b - - 0 1 moves a8a7
go perft 1
UCI
)
assert_not_contains "${out}" '^c1d2e3: 1$'

# The previous two-leg capture records a Lion on its via square as well as its final victim.
out=$(run_uci "${ENGINE}" "${FSX_TMP_INI}" chu-multi-test <<'UCI'
position fen 8/8/8/8/8/r1L1p3/3l4/2H5 w - - 0 1 moves c1d2e3
go perft 1
UCI
)
assert_not_contains "${out}" '^a3c3: 1$'

# Hidden/x-ray protector: the White cub on b3 blocks the Black rook a3 from
# the Black Lion d3. The cub takes a pawn on c3 then the Lion (b3c3d3); after
# b3 vacates, the rook x-rays d3, so the capture is illegal even though d3
# looks undefended before the move.
out=$(run_uci "${ENGINE}" "${FSX_TMP_INI}" chu-xray-test <<'UCI'
position fen 8/8/8/8/8/rMpl4/8/8 w - - 0 1
go perft 1
UCI
)
assert_not_contains "${out}" '^b3c3d3: 1$'

# Control: without the rook the same distant Lion capture is legal.
out=$(run_uci "${ENGINE}" "${FSX_TMP_INI}" chu-xray-test <<'UCI'
position fen 8/8/8/8/8/1Mpl4/8/8 w - - 0 1
go perft 1
UCI
)
assert_contains "${out}" '^b3c3d3: 1$'
