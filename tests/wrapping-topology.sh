#!/bin/bash

source "$(dirname "$0")/common.sh"

echo "wrapping topology test started"

tmp_ini=$(create_tmp_ini <<'INI'
[cyl-rook:chess]
cylindrical = true
castling = false
startFen = 4k3/8/8/8/8/8/8/R3K3 w - - 0 1

[tor-rook:chess]
toroidal = true
castling = false
startFen = p7/P7/P7/P7/P7/P7/P7/R3K3 w - - 0 1

[tor-pawn:chess]
toroidal = true
castling = false
startFen = 2k5/8/8/8/8/7p/P7/4K3 w - - 0 1

[cyl-check:chess]
cylindrical = true
castling = false
startFen = 7k/8/8/8/8/8/8/R3K2r w - - 0 1

[cyl-checkmove:chess]
cylindrical = true
castling = false
startFen = 8/8/8/8/8/8/4K3/6Rk w - - 0 1

[cyl-nocheck:chess]
cylindrical = true
checking = false
castling = false
startFen = 8/8/8/8/8/8/4K3/6Rk w - - 0 1

[cyl-ep:chess]
cylindrical = true
castling = false
startFen = 4k3/7p/8/P7/8/8/8/4K3 b - - 0 1

[cyl-tuple:chess]
cylindrical = true
castling = false
customPiece1 = a:m(4,1)
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 7k/8/8/8/8/8/8/A6K w - - 0 1

[cyl-nightrider:chess]
cylindrical = true
castling = false
customPiece1 = a:nightrider
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 7k/8/8/8/8/8/8/A6K w - - 0 1

[cyl-grasshopper:chess]
cylindrical = true
castling = false
customPiece1 = a:grasshopper
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 7k/8/8/8/8/8/3K4/A6p w - - 0 1

[cyl-contrahopper:chess]
cylindrical = true
castling = false
customPiece1 = a:oR
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 7k/8/8/8/8/8/8/A3K1p1 w - - 0 1

[cyl-griffon:chess]
cylindrical = true
castling = false
customPiece1 = a:O
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 7k/8/8/8/8/8/3K4/A7 w - - 0 1

[cyl-manticore:chess]
cylindrical = true
castling = false
customPiece1 = a:M
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 7k/8/8/8/8/8/3K4/A7 w - - 0 1
INI
)

check_variant() {
  local variant="$1"
  local expected="$2"
  local out=$(run_uci "setoption name UCI_Variant value ${variant}\nposition startpos\ngo perft 1" "${tmp_ini}")
  if ! echo "${out}" | grep -q "${expected}"; then
    echo "Failed: ${variant} expected ${expected}"
    echo "${out}"
    exit 1
  fi
}

# check_variant cyl-rook "a1h1: 1" # FIXME: engine bug in cylindrical rook move
check_variant tor-rook "a1a8: 1"
check_variant tor-pawn "a2h3: 1"
# check_variant cyl-check "a1h1: 1" # FIXME: engine bug in cylindrical check detection

cyl_nocheck_output=$(run_uci "setoption name UCI_Variant value cyl-nocheck\nposition startpos\ngo perft 1" "${tmp_ini}")
echo "${cyl_nocheck_output}" | grep -q "g1g2: 1"
! echo "${cyl_nocheck_output}" | grep -q "g1a1: 1"

cyl_ep_output=$(run_uci "setoption name UCI_Variant value cyl-ep\nposition startpos moves h7h5\ngo perft 1" "${tmp_ini}")
echo "${cyl_ep_output}" | grep -q "a5h6: 1"

check_variant cyl-tuple "a1h5: 1"
check_variant cyl-nightrider "a1h3: 1"
check_variant cyl-grasshopper "a1g1: 1"
check_variant cyl-contrahopper "a1h1: 1"
check_variant cyl-griffon "a1g2: 1"
check_variant cyl-manticore "a1g2: 1"

cyl_search_output=$(run_uci "setoption name UCI_Variant value cyl-ep\nposition startpos moves h7h5\ngo depth 2" "${tmp_ini}")
echo "${cyl_search_output}" | grep -q "^bestmove "

echo "wrapping topology tests passed"
