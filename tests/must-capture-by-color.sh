#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
source "${SCRIPT_DIR}/lib/uci.sh"

tmp_ini="$(mktemp)"
trap 'rm -f "$tmp_ini"' EXIT

cat > "$tmp_ini" <<'INI'
[asymmustcapture:chess]
mustCaptureWhite = true
mustCaptureBlack = false
startFen = 4k3/8/8/3p4/4P3/8/8/4K3 w - - 0 1
INI

out_white=$(run_uci "${1:-"${ROOT_DIR}/src/stockfish"}" "$tmp_ini" asymmustcapture <<'UCI'
position startpos
go perft 1
UCI
)
assert_contains "$out_white" "e4d5:"
assert_not_contains "$out_white" "e4e5:"

assert_nodes "$out_white" 1

black_fen='4k3/8/8/4p3/3P4/8/8/4K3 b - - 0 1'
out_black=$(run_uci "${1:-"${ROOT_DIR}/src/stockfish"}" "$tmp_ini" asymmustcapture <<UCI
position fen ${black_fen}
go perft 1
UCI
)
assert_contains "$out_black" "e5d4:"
assert_contains "$out_black" "e5e4:"

echo "mustCaptureByColor test OK"
