#!/usr/bin/env bash

set -euo pipefail

source "$(cd "$(dirname "$0")/../../../.." && pwd)/tests/lib/uci.sh"
setup_test_context "${1:-}" "${2:-}" "movement rule matrix"

custom_variants=$(mktemp "${TMPDIR:-/tmp}/fsx-movement-matrix-XXXXXX.ini")
trap 'rm -f "$custom_variants"' EXIT
cat >"$custom_variants" <<'INI'
[strongpawnproto:chess]
customPiece1 = t:W
customPiece2 = s:ffN
customPiece3 = c:F
customPiece4 = u:K
startFen = rnbqkbnr/tscuucst/8/8/8/8/TSCUUCST/RNBQKBNR w - - 0 1
promotedPieceType = t:r s:n c:b u:q
mandatoryPiecePromotion = true
doubleStep = false
castling = false
enPassantTypes = -

[altergaproto:chess]
customPiece1 = n:mNcB
customPiece2 = b:fWmFcB
customPiece3 = r:mWcRfF
customPiece4 = q:BmRcN
customPiece5 = k:FmWisR2cN
castling = false
startFen = rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w - - 0 1

[caissapathoff:chess]
king = q:Q
castling = false
startFen = q7/8/8/8/8/8/1r6/4Q3 w - - 0 1

[caissapathon:caissapathoff]
royalPieceNoThroughCheck = true

[whitewalls:chess]
maxRank = 4
maxFile = 4
pieceToCharTable = -
king = -
queen = -
customPiece1 = q:mQ
startFen = 4/1q2/4/2Q1 w - - 0 1
wallingRule = arrow
wallingWhite = true
wallingBlack = false
captureForbidden = *:*
checking = false

[droplast:chess]
startFen = 8/8/8/8/8/8/8/8[KNkn] w - - 0 1
pieceDrops = true
mustDrop = true
passUntilSetup = true
dropKingLast = true
castling = false
checking = false

[pairedpoints:chess]
startFen = 8/8/8/8/8/8/8/8[NNnn] w - - 0 1
pieceDrops = true
symmetricDropTypes = n
payPointsToDrop = true
piecePoints = n:5

INI

out=$(run_uci "$ENGINE" "$custom_variants" strongpawnproto <<'UCI'
position startpos
go perft 1
UCI
)
assert_contains_literal "$out" "b2a4: 1" "strong pawn forward knight leap"
assert_contains_literal "$out" "a2a3: 1" "strong pawn one-step move"
assert_not_contains_literal "$out" "e1g1: 1" "castling disabled for strong pawn"

out=$(run_uci "$ENGINE" "$custom_variants" altergaproto <<'UCI'
position fen 4k3/8/5p2/3p4/4N3/8/8/4K3 w - - 0 1
go perft 1
UCI
)
assert_contains_literal "$out" "e4d5: 1" "Alterga bishop-style capture"
assert_contains_literal "$out" "e4c5: 1" "Alterga knight-style quiet move"
assert_not_contains_literal "$out" "e4f6: 1" "Alterga capture modality"

off=$(run_uci "$ENGINE" "$custom_variants" caissapathoff <<'UCI'
position startpos
go perft 1
UCI
)
on=$(run_uci "$ENGINE" "$custom_variants" caissapathon <<'UCI'
position startpos
go perft 1
UCI
)
assert_contains_literal "$off" "e1e3: 1" "royal piece baseline path"
assert_not_contains_literal "$on" "e1e3: 1" "royal piece through-check restriction"
assert_contains_literal "$on" "e1f1: 1" "royal piece safe path"

out=$(run_uci "$ENGINE" "$custom_variants" whitewalls <<'UCI'
position startpos
go perft 1
UCI
)
assert_contains_literal "$out" "," "white wall move annotation"

out=$(run_uci "$ENGINE" "$custom_variants" droplast <<'UCI'
position startpos
go perft 1
UCI
)
assert_contains_literal "$out" "N@" "drop king last starts with non-king drops"
assert_not_contains_literal "$out" "K@" "drop king last suppresses king drops"

out=$(run_uci "$ENGINE" "$custom_variants" pairedpoints <<'UCI'
position fen 8/8/8/8/8/8/8/8[NNnn] w - - 0 1 {10 10}
go perft 1
UCI
)
assert_contains_literal "$out" "," "paired drop with sufficient points"

out=$(run_uci "$ENGINE" "$VARIANTS" ichess <<'UCI'
position startpos
go perft 1
UCI
)
assert_contains_literal "$out" "N@" "iChess setup drops"
assert_not_contains_literal "$out" "K@" "iChess setup king is not dropped first"

out=$(run_uci "$ENGINE" "$VARIANTS" snort <<'UCI'
position startpos
go perft 1
UCI
)
assert_contains_literal "$out" "Nodes searched:" "snort has legal opening moves"
assert_not_contains_literal "$out" "Nodes searched: 0" "snort is not incorrectly adjudicated by material"

out=$(run_uci "$ENGINE" "$VARIANTS" konane <<'UCI'
position fen MmMmMmMmMm/mMmMmMmMmM/MmMmMmMmMm/mMmMmMmMmM/MmMmMmMmMm/mMmMmMmMmM/MmMmMmMmMm/mMmMmMmMmM/MmMmMmMmMm/mMmMmMmMmM w - - 0 1
go perft 1
UCI
)
assert_contains_literal "$out" "Nodes searched:" "Konane opening has removals"
assert_not_contains_literal "$out" "0000: 1" "Konane removals are not rendered as passes"

out=$(run_uci "$ENGINE" "$VARIANTS" checkers <<'UCI'
position fen 8/8/5m2/4M3/8/2K5/8/7K b - - 0 1 moves f6d4 0000
d
UCI
)
assert_contains_literal "$out" "Fen:" "checkers forced continuation position loaded"
assert_contains_literal "$out" "K" "promoted checkers king survives a pass"

out=$(run_uci "$ENGINE" "$VARIANTS" checkers <<'UCI'
position fen 8/2m1m3/1M6/8/8/8/8/8 w - - 0 1
go perft 1
UCI
)
assert_contains_literal "$out" "b6d8k: 1" "checkers capture crowns a man"

out=$(run_uci "$ENGINE" "$VARIANTS" checkers <<'UCI'
position fen 8/2m1m3/1M6/8/8/8/8/8 w - - 0 1 moves b6d8k
d
UCI
)
assert_contains_literal "$out" "Fen: 3K4/4m3/8/8/8/8/8/8 b - - 0 1" "checkers crowning ends the capture turn"

out=$(run_uci "$ENGINE" "$VARIANTS" checkers <<'UCI'
position fen 1M6/8/8/8/8/8/8/8 w - - 0 1
go perft 1
UCI
)
assert_contains_literal "$out" "Nodes searched: 0" "checkers side with no legal move loses"

out=$(run_uci "$ENGINE" "$VARIANTS" checkers <<'UCI'
position fen 8/8/5k2/8/8/2K5/8/8 w - - 0 1 moves c3b4 f6g5 b4c3 g5f6 c3b4 f6g5 b4c3 g5f6
go depth 1
UCI
)
assert_not_contains_literal "$out" "info depth 1" "checkers threefold repetition is adjudicated as a draw"

echo "movement rule matrix cases passed"
