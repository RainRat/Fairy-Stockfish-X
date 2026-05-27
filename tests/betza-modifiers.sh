#!/bin/bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

ENGINE=$(default_engine "${1:-}")

tmp_ini=$(mktemp)
trap 'rm -f "$tmp_ini"' EXIT

cat > "$tmp_ini" <<'INI'
[modsugar_ski_group:chess]
customPiece1 = a:j(RB)
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 4k3/8/8/8/3A4/8/8/4K3 w - - 0 1

[modsugar_ski_explicit:chess]
customPiece1 = a:jRjB
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 4k3/8/8/8/3A4/8/8/4K3 w - - 0 1

[modsugar_max_group:chess]
customPiece1 = a:z(RB)
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 4k3/8/8/8/3A4/8/8/4K3 w - - 0 1

[modsugar_max_explicit:chess]
customPiece1 = a:zRzB
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 4k3/8/8/8/3A4/8/8/4K3 w - - 0 1

[ski_autocheck:chess]
customPiece1 = s:jR
pieceToCharTable = -
startFen = 4k3/4S3/8/8/8/8/8/4K3 w - - 0 1

[dist10:chess]
customPiece1 = a:R10
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 7k/8/8/4A3/8/8/8/K7 w - - 0 1

[tuplewarn:chess]
customPiece1 = a:j(2,1)
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 4k3/8/8/8/3A4/8/8/4K3 w - - 0 1
INI

perft_moves() {
  local variant=$1
  run_uci "$ENGINE" "$tmp_ini" "$variant" <<'UCI' | grep ':'
position startpos
go perft 1
UCI
}

cmp <(perft_moves modsugar_ski_group) <(perft_moves modsugar_ski_explicit)
cmp <(perft_moves modsugar_max_group) <(perft_moves modsugar_max_explicit)

dist_out=$(run_uci "$ENGINE" "$tmp_ini" dist10 <<'UCI'
position startpos
go perft 1
UCI
)
assert_contains "$dist_out" "e5e8:"
assert_contains "$dist_out" "e5h5:"

check_out=$(run_uci "$ENGINE" "$tmp_ini" tuplewarn <<'UCI' 2>&1
UCI
)
assert_contains "$check_out" "Unsupported Betza tuple modifier combination"

ski_out=$(run_uci "$ENGINE" "$tmp_ini" ski_autocheck <<'UCI'
position startpos moves e7e5
d
UCI
)
assert_contains "$ski_out" 'Checkers: e5 '

echo "betza-modifiers test OK"
