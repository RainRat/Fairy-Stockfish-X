#!/bin/bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

ENGINE=$(default_engine "${1:-}")

TMP_VARIANT_PATH=$(mktemp "${TMPDIR:-/tmp}/fsx-hex-pieces-XXXXXX.ini")
trap 'rm -f "${TMP_VARIANT_PATH}"' EXIT

cat >"${TMP_VARIANT_PATH}" <<'INI'
[hex-rook-test:fairy]
maxRank = 5
maxFile = e
hexBoard = true
pieceToCharTable = RKBQNPX.rkbqnp.x
king = -
customPiece1 = r:RrfBlbB
customPiece2 = k:WrfFlbF
customPiece3 = b:flBrbBrf(2,1)lb(2,1)fr(2,1)bl(2,1)
customPiece4 = q:RrfBlbBflBrbBrf(2,1)lb(2,1)fr(2,1)bl(2,1)
customPiece5 = n:fl(2,1)lb(2,1)fr(2,1)rb(2,1)rf(2,1)bl(2,1)fl(1,2)lb(1,2)fr(1,2)rb(1,2)rf(1,2)bl(1,2)
customPiece6 = p:mfWclFcrF
customPiece7 = x:WrfFlbF
startFen = 5/5/2R2/5/5 w - - 0 1

[hex-king-test:hex-rook-test]
startFen = 5/5/2K2/5/5 w - - 0 1

[hex-bishop-test:hex-rook-test]
startFen = 5/5/2B2/5/5 w - - 0 1

[hex-queen-test:hex-rook-test]
startFen = 5/5/2Q2/5/5 w - - 0 1

[hex-knight-test:hex-rook-test]
maxRank = 7
maxFile = g
startFen = 7/7/7/3N3/7/7/7 w - - 0 1

[hex-pawn-test:hex-rook-test]
maxRank = 7
maxFile = g
startFen = 7/7/7/3P3/2x1x2/7/7 w - - 0 1

[hex-royal-king-test:hex-rook-test]
maxRank = 7
maxFile = g
king = k:WrfFlbFflFrbFrf(2,1)lb(2,1)fr(2,1)bl(2,1)
startFen = 6k/7/7/3K3/7/7/7 w - - 0 1
INI

out=$(run_uci "$ENGINE" "$TMP_VARIANT_PATH" hex-rook-test <<'UCI'
position startpos
go perft 1
UCI
)
assert_nodes "$out" 12
assert_contains "$out" "^c3a1: 1$"
assert_contains "$out" "^c3c1: 1$"
assert_contains "$out" "^c3b2: 1$"
assert_contains "$out" "^c3c2: 1$"
assert_contains "$out" "^c3a3: 1$"
assert_contains "$out" "^c3b3: 1$"
assert_contains "$out" "^c3d3: 1$"
assert_contains "$out" "^c3e3: 1$"
assert_contains "$out" "^c3c4: 1$"
assert_contains "$out" "^c3d4: 1$"
assert_contains "$out" "^c3c5: 1$"
assert_contains "$out" "^c3e5: 1$"

out=$(run_uci "$ENGINE" "$TMP_VARIANT_PATH" hex-king-test <<'UCI'
position startpos
go perft 1
UCI
)
assert_nodes "$out" 6
assert_contains "$out" "^c3b2: 1$"
assert_contains "$out" "^c3c2: 1$"
assert_contains "$out" "^c3b3: 1$"
assert_contains "$out" "^c3d3: 1$"
assert_contains "$out" "^c3c4: 1$"
assert_contains "$out" "^c3d4: 1$"

out=$(run_uci "$ENGINE" "$TMP_VARIANT_PATH" hex-bishop-test <<'UCI'
position startpos
go perft 1
UCI
)
assert_nodes "$out" 8
assert_contains "$out" "^c3b1: 1$"
assert_contains "$out" "^c3e1: 1$"
assert_contains "$out" "^c3a2: 1$"
assert_contains "$out" "^c3d2: 1$"
assert_contains "$out" "^c3b4: 1$"
assert_contains "$out" "^c3e4: 1$"
assert_contains "$out" "^c3a5: 1$"
assert_contains "$out" "^c3d5: 1$"

out=$(run_uci "$ENGINE" "$TMP_VARIANT_PATH" hex-queen-test <<'UCI'
position startpos
go perft 1
UCI
)
assert_nodes "$out" 20
assert_contains "$out" "^c3a1: 1$"
assert_contains "$out" "^c3e5: 1$"
assert_contains "$out" "^c3b1: 1$"
assert_contains "$out" "^c3e1: 1$"
assert_contains "$out" "^c3a2: 1$"
assert_contains "$out" "^c3d5: 1$"

out=$(run_uci "$ENGINE" "$TMP_VARIANT_PATH" hex-knight-test <<'UCI'
position startpos
go perft 1
UCI
)
assert_nodes "$out" 8
assert_contains "$out" "^d4c2: 1$"
assert_contains "$out" "^d4e2: 1$"
assert_contains "$out" "^d4b3: 1$"
assert_contains "$out" "^d4f3: 1$"
assert_contains "$out" "^d4b5: 1$"
assert_contains "$out" "^d4f5: 1$"
assert_contains "$out" "^d4c6: 1$"
assert_contains "$out" "^d4e6: 1$"

out=$(run_uci "$ENGINE" "$TMP_VARIANT_PATH" hex-pawn-test <<'UCI'
position startpos
go perft 1
UCI
)
assert_nodes "$out" 3
assert_contains "$out" "^d4c3: 1$"
assert_contains "$out" "^d4d5: 1$"
assert_contains "$out" "^d4e3: 1$"

out=$(run_uci "$ENGINE" "$TMP_VARIANT_PATH" hex-royal-king-test <<'UCI'
position startpos
go perft 1
UCI
)
assert_nodes "$out" 10
assert_contains "$out" "^d4c2: 1$"
assert_contains "$out" "^d4b3: 1$"
assert_contains "$out" "^d4c3: 1$"
assert_contains "$out" "^d4d3: 1$"
assert_contains "$out" "^d4e3: 1$"
assert_contains "$out" "^d4c4: 1$"
assert_contains "$out" "^d4e4: 1$"
assert_contains "$out" "^d4c5: 1$"
assert_contains "$out" "^d4d5: 1$"
assert_contains "$out" "^d4e5: 1$"

echo "hex piece movement regression passed"
