#!/bin/bash

set -euo pipefail

error() {
  echo "wrapping topology test failed on line $1"
  [[ -n "${TMP_VARIANT_PATH:-}" ]] && rm -f "${TMP_VARIANT_PATH}"
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
ENGINE="${1:-${REPO_ROOT}/src/stockfish}"

TMP_VARIANT_PATH=$(mktemp /tmp/fsx-wrap-XXXXXX.ini)
cat >"${TMP_VARIANT_PATH}" <<'INI'
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

run_variant() {
  local variant="$1"
  cat <<CMDS | "${ENGINE}" 2>&1
uci
setoption name VariantPath value ${TMP_VARIANT_PATH}
setoption name UCI_Variant value ${variant}
position startpos
go perft 1
quit
CMDS
}

cyl_output=$(run_variant cyl-rook)
echo "${cyl_output}" | grep -q "a1h1: 1"

tor_rook_output=$(run_variant tor-rook)
echo "${tor_rook_output}" | grep -q "a1a8: 1"

tor_pawn_output=$(run_variant tor-pawn)
echo "${tor_pawn_output}" | grep -q "a2h3: 1"

cyl_check_output=$(run_variant cyl-check)
echo "${cyl_check_output}" | grep -q "a1h1: 1"

cyl_nocheck_output=$(run_variant cyl-nocheck)
echo "${cyl_nocheck_output}" | grep -q "g1g2: 1"
! echo "${cyl_nocheck_output}" | grep -q "g1a1: 1"

cyl_ep_output=$(cat <<CMDS | "${ENGINE}" 2>&1
uci
setoption name VariantPath value ${TMP_VARIANT_PATH}
setoption name UCI_Variant value cyl-ep
position startpos moves h7h5
go perft 1
quit
CMDS
)
echo "${cyl_ep_output}" | grep -q "a5h6: 1"

cyl_tuple_output=$(run_variant cyl-tuple)
echo "${cyl_tuple_output}" | grep -q "a1h5: 1"

cyl_nightrider_output=$(run_variant cyl-nightrider)
echo "${cyl_nightrider_output}" | grep -q "a1h3: 1"

cyl_grasshopper_output=$(run_variant cyl-grasshopper)
echo "${cyl_grasshopper_output}" | grep -q "a1g1: 1"

cyl_contrahopper_output=$(run_variant cyl-contrahopper)
echo "${cyl_contrahopper_output}" | grep -q "a1h1: 1"

cyl_griffon_output=$(run_variant cyl-griffon)
echo "${cyl_griffon_output}" | grep -q "a1g2: 1"

cyl_manticore_output=$(run_variant cyl-manticore)
echo "${cyl_manticore_output}" | grep -q "a1g2: 1"

cyl_search_output=$(cat <<CMDS | "${ENGINE}" 2>&1
uci
setoption name VariantPath value ${TMP_VARIANT_PATH}
setoption name UCI_Variant value cyl-ep
position startpos moves h7h5
go depth 2
quit
CMDS
)
echo "${cyl_search_output}" | grep -q "^bestmove "

python3 - <<'PY'
import pyffish as sf

sf.load_variant_config(
    """
[cyl-checkmove:chess]
cylindrical = true
castling = false
startFen = 8/8/8/8/8/8/4K3/6Rk w - - 0 1
"""
)

fen = "8/8/8/8/8/8/4K3/6Rk w - - 0 1"
assert not sf.gives_check("cyl-checkmove", fen, [])
assert sf.gives_check("cyl-checkmove", fen, ["g1a1"])
PY

rm -f "${TMP_VARIANT_PATH}"
unset TMP_VARIANT_PATH

echo "wrapping topology tests passed"
