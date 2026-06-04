#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "wrapped connect win regressions"

load_inline_variants <<'INI'
[wrap-connect:chess]
cylindrical = true
connectN = 4
connectPieceTypes = p
connectHorizontal = true
connectVertical = false
connectDiagonal = false
startFen = 4k3/8/8/4K3/8/8/8/PPP4P b - - 0 1

[wrap-collinear:chess]
toroidal = true
connectPieceTypes = p
collinearN = 4
connectHorizontal = false
connectVertical = false
connectDiagonal = true
connectNorthEast = true
connectSouthEast = false
startFen = 3k3P/8/8/4K3/8/2P5/1P6/P7 b - - 0 1

[plain-connect:chess]
connectN = 4
connectPieceTypes = p
connectHorizontal = true
connectVertical = false
connectDiagonal = false
startFen = 4k3/8/8/4K3/8/8/8/PPPP4 b - - 0 1
INI
tmp_ini="${FSX_TMP_INI}"

echo "wrapped connect win regressions started"

out=$(run_uci "$ENGINE" "$tmp_ini" wrap-connect <<'UCI'
position startpos
go perft 1
UCI
)
assert_nodes "$out" 0

out=$(run_uci "$ENGINE" "$tmp_ini" wrap-collinear <<'UCI'
position startpos
go perft 1
UCI
)
assert_nodes "$out" 0

out=$(run_uci "$ENGINE" "$tmp_ini" plain-connect <<'UCI'
position startpos
go perft 1
UCI
)
assert_nodes "$out" 0

echo "wrapped connect win regressions passed"
