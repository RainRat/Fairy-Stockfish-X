#!/bin/bash

set -euo pipefail

error() {
  echo "fairy notation regressions failed on line $1" >&2
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

ENGINE=$(default_engine "${1:-}")

tmp_ini=$(mktemp)
trap 'rm -f "${tmp_ini}"' EXIT

cat >"${tmp_ini}" <<'INI'
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

[betzarifle:chess]
customPiece1 = a:R^

[betzaplain:chess]
customPiece1 = a:R

[alias-wazir:chess]
customPiece1 = a:wazir
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 7k/8/8/8/3A4/8/8/K7 w - - 0 1

[alias-wazir-ref:chess]
customPiece1 = a:W
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 7k/8/8/8/3A4/8/8/K7 w - - 0 1

[alias-nightrider:chess]
customPiece1 = a:nightrider
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 7k/8/8/8/3A4/8/8/K7 w - - 0 1

[alias-nightrider-ref:chess]
customPiece1 = a:NN
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 7k/8/8/8/3A4/8/8/K7 w - - 0 1

[alias-grasshopper:chess]
customPiece1 = a:grasshopper
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 7k/8/8/8/3A4/8/8/K7 w - - 0 1

[alias-grasshopper-ref:chess]
customPiece1 = a:gQ
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 7k/8/8/8/3A4/8/8/K7 w - - 0 1

[alias-marshall:chess]
customPiece1 = a:marshall
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 7k/8/8/8/3A4/8/8/K7 w - - 0 1

[alias-marshall-ref:chess]
customPiece1 = a:RN
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 7k/8/8/8/3A4/8/8/K7 w - - 0 1

[alias-cardinal:chess]
customPiece1 = a:cardinal
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 7k/8/8/8/3A4/8/8/K7 w - - 0 1

[alias-cardinal-ref:chess]
customPiece1 = a:BN
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 7k/8/8/8/3A4/8/8/K7 w - - 0 1
INI

perft_moves() {
  local variant="$1"
  run_uci "$ENGINE" "$tmp_ini" "$variant" <<'EOF' | grep -E '^[a-z][0-9]+[a-z][0-9]+:'
position startpos
go perft 1
EOF
}

echo "fairy notation regressions started"

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

out=$(run_uci "$ENGINE" "$tmp_ini" range35 <<'UCI'
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

out=$(run_uci "$ENGINE" "$tmp_ini" range3plus <<'UCI'
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

invalid_out=$(run_uci "$ENGINE" "$tmp_ini" rangeinvalid <<'UCI' 2>&1
UCI
)
assert_contains "$invalid_out" "Invalid Betza rider range"

rifle_moves=$(run_uci "$ENGINE" "$tmp_ini" betzarifle <<'UCI'
position fen p3k3/8/8/8/8/8/8/A3K3 w - - 0 1
go perft 1
UCI
)
assert_contains "$rifle_moves" "^a1a8: 1$"

plain_after="$(run_uci "$ENGINE" "$tmp_ini" betzaplain <<'UCI'
position fen p3k3/8/8/8/8/8/8/A3K3 w - - 0 1 moves a1a8
d
UCI
)"
assert_contains_literal "$plain_after" "Fen: A3k3/8/8/8/8/8/8/4K3 b - - 0 1"

rifle_after="$(run_uci "$ENGINE" "$tmp_ini" betzarifle <<'UCI'
position fen p3k3/8/8/8/8/8/8/A3K3 w - - 0 1 moves a1a8
d
UCI
)"
assert_contains_literal "$rifle_after" "Fen: 4k3/8/8/8/8/8/8/A3K3 b - - 0 1"

cmp <(perft_moves alias-wazir) <(perft_moves alias-wazir-ref)
cmp <(perft_moves alias-nightrider) <(perft_moves alias-nightrider-ref)
cmp <(perft_moves alias-grasshopper) <(perft_moves alias-grasshopper-ref)
cmp <(perft_moves alias-marshall) <(perft_moves alias-marshall-ref)
cmp <(perft_moves alias-cardinal) <(perft_moves alias-cardinal-ref)

echo "fairy notation regressions passed"
