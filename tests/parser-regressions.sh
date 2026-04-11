#!/bin/bash

set -euo pipefail

error() {
  echo "parser regression test failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
ENGINE=${1:-${SCRIPT_DIR}/../src/stockfish}

tmp_ini=$(mktemp)
trap 'rm -f "${tmp_ini}"' EXIT

cat > "${tmp_ini}" <<'INI'
[ptbg-no-semicolon:chess]
pieceDrops = true
dropRegionWhite = P(a8)

[tuple-nonsquare:chess]
maxRank = 8
maxFile = 5
castling = false
doubleStep = false
customPiece1 = a:m(7,0)
startFen = 5/5/5/5/5/5/5/A4 w - - 0 1

[parse-error-empty-fields:chess]
piecePoints =
promotionLimit =
priorityDropTypes =
virtualDropLimit =

[named-custom-piece-hint:chess]
falcon = a:W

[walling-seirawan:chess]
wallingRule = duck
seirawanGating = true

[walling-potions:chess]
wallingRule = duck
potions = true

[duck-petrify:chess]
wallingRule = duck
petrifyOnCaptureTypes = p

[walling-freedrops:chess]
wallingRule = duck
freeDrops = true

[toroidal-connect:chess]
toroidal = true
connectN = 4

[toroidal-maxrider:chess]
toroidal = true
customPiece1 = a:mzQ

[cylindrical-collinear:chess]
cylindrical = true
collinearN = 3

[adjacent-castling-warning:chess]
maxRank = 6
maxFile = f
castling = true
castlingKingFile = d
castlingKingsideFile = e
castlingQueensideFile = b
startFen = rbnkbr/pppppp/6/6/PPPPPP/RBNKBR w KQkq - 0 1

[promotion-by-file-inherit:chess]
promotionPieceTypes = a:q b:r
promotionPieceTypesWhite = a:n
startFen = 8/1P6/8/8/8/8/8/4k2K w - - 0 1

[promotion-by-file-spaces:chess]
promotionPieceTypes = a:q b:r c:b d:n e:- f:-
startFen = 8/1P6/8/8/8/8/8/4k2K w - - 0 1

[remove-connect-conn:fairy]
maxRank = 3
maxFile = 3
connectN = 3
removeConnectN = 3
pieceToCharTable = -
king = -
immobile = p
startFen = 3/3/3[PPPPPpppp] w - - 0 1
pieceDrops = true

[remove-connect-pseudoroyal:fairy]
maxRank = 3
maxFile = 3
removeConnectN = 3
pieceToCharTable = -
king = -
pseudoRoyalTypes = p
startFen = 3/3/P2 w - - 0 1

[hex-weak-crosscut:fairy]
maxRank = 5
maxFile = 5
hexBoard = true
pieceToCharTable = -
king = -
pieceDrops = true
mustDrop = true
customPiece1 = s:m
weakCrosscutDropIllegal = true
startFen = ****1/***2/**3/*4/5[SSSSSSSSSSSSSSSsssssssssssssss] b - - 0 1
INI

echo "parser regression tests started"

check_output=$("${ENGINE}" check "${tmp_ini}" 2>&1 || true)
if echo "${check_output}" | grep -Eq "PieceTypeBitboardGroup declaration|Invalid value.*dropRegionWhite|Error parsing|unterminated"; then
  echo "${check_output}"
  exit 1
fi

if printf '%s\n' "${check_output}" | grep -Eq "piecePoints - Invalid piece type: $|promotionLimit - Invalid piece type: $|priorityDropTypes - Invalid piece type: $|virtualDropLimit - Invalid piece type: $"; then
  echo "${check_output}"
  exit 1
fi

verify_warning() {
  local pattern="$1"
  local label="$2"
  if ! printf '%s\n' "${check_output}" | grep -qF "${pattern}"; then
    echo "Failed: ${label}"
    echo "Expected warning not found: ${pattern}"
    printf '%s\n' "${check_output}"
    exit 1
  fi
}

verify_warning "wallingRule and seirawanGating are incompatible." "seirawanGating check"
verify_warning "wallingRule and potions are incompatible." "potions check"
verify_warning "wallingRule=duck and petrifyOnCaptureTypes are incompatible." "petrify check"
verify_warning "pieceDrops and any walling are incompatible." "freeDrops check"
verify_warning "falcon looks like a custom piece definition. Use customPieceN = a:W for new custom pieces." "named custom piece hint"
verify_warning "Wrapped boards do not support connect/collinear win conditions." "wrapped connect rejection"
verify_warning "Wrapped boards do not support x/z rider modifiers in customPiece1." "toroidal x/z rejection"
verify_warning "Castling destination is adjacent to castlingKingFile; some GUIs/protocols may not distinguish castling from a normal king move." "adjacent castling warning"
verify_warning "removeConnectN is incompatible with connection win conditions." "removeConnectN connect rejection"
verify_warning "removeConnectN is incompatible with (pseudo/anti-)royal pieces." "removeConnectN royal rejection"
verify_warning "Hex boards do not support square weak-connection drop rules." "hex weak-link rejection"

nonking_ini=$(mktemp)
trap 'rm -f "${tmp_ini}" "${nonking_ini}"' EXIT
cat > "${nonking_ini}" <<'INI'
[nonking-inline-betza:chess]
rook = r:R3
INI

nonking_output=$("${ENGINE}" check "${nonking_ini}" 2>&1 || true)
if ! printf '%s\n' "${nonking_output}" | grep -qF "rook only supports a piece letter here. Use customPieceN = r:R3 and remap rook to that letter instead."; then
  echo "Failed: non-king inline Betza rejection"
  printf '%s\n' "${nonking_output}"
  exit 1
fi

tuple_output=$(cat <<CMDS | "${ENGINE}" 2>&1
uci
setoption name VariantPath value ${tmp_ini}
setoption name UCI_Variant value tuple-nonsquare
position startpos
go perft 1
quit
CMDS
)

if echo "${tuple_output}" | grep -q "No piece char found for custom piece"; then
  echo "${tuple_output}"
  exit 1
fi

promotion_file_output=$(cat <<CMDS | "${ENGINE}" 2>&1
uci
setoption name VariantPath value ${tmp_ini}
setoption name UCI_Variant value promotion-by-file-inherit
position startpos
go perft 1
quit
CMDS
)

if ! echo "${promotion_file_output}" | grep -q "b7b8r:"; then
  echo "${promotion_file_output}"
  exit 1
fi

if echo "${promotion_file_output}" | grep -q "b7b8q:"; then
  echo "${promotion_file_output}"
  exit 1
fi

if echo "${promotion_file_output}" | grep -q "b7b8n:"; then
  echo "${promotion_file_output}"
  exit 1
fi

promotion_spaces_output=$(cat <<CMDS | "${ENGINE}" 2>&1
uci
setoption name VariantPath value ${tmp_ini}
setoption name UCI_Variant value promotion-by-file-spaces
position startpos
go perft 1
quit
CMDS
)

if ! echo "${promotion_spaces_output}" | grep -q "b7b8r:"; then
  echo "${promotion_spaces_output}"
  exit 1
fi

terminal_output=$(cat <<'CMDS' | "${ENGINE}" 2>&1
uci
position fen 7k/5Q2/7K/8/8/8/8/8 b - - 0 1
go depth 1
quit
CMDS
)

if ! echo "${terminal_output}" | grep -q "bestmove (none)"; then
  echo "${terminal_output}"
  exit 1
fi

bench_output=$("${ENGINE}" bench 16 1 1 default nonsense 2>&1 || true)
if ! echo "${bench_output}" | grep -q "Nodes searched  : "; then
  echo "${bench_output}"
  exit 1
fi

castling_diag_output=$(python3 - <<'PY' 2>&1
import pyffish

pyffish.load_variant_config(
    """
[castdiag-empty:chess]
maxFile = j
castling = true
castlingKingFile = f
castlingKingsideFile = i
castlingQueensideFile = c
castlingRookKingsideFile = j
castlingRookQueensideFile = b
startFen = 10/10/10/10/10/10/10/1R3K2R1 w JQ - 0 1

[castdiag-wrongpiece:chess]
maxFile = j
castling = true
castlingKingFile = f
castlingKingsideFile = i
castlingQueensideFile = c
castlingRookKingsideFile = j
castlingRookQueensideFile = b
startFen = 10/10/10/10/10/10/10/1R3K3N w JQ - 0 1

[castdiag-single-rook:chess]
castling = true
startFen = 8/8/8/8/8/8/8/R3K3 w KQ - 0 1
"""
)

for fen, variant in [
    ("10/10/10/10/10/10/10/1R3K2R1 w JQ - 0 1", "castdiag-empty"),
    ("10/10/10/10/10/10/10/1R3K3N w JQ - 0 1", "castdiag-wrongpiece"),
    ("8/8/8/8/8/8/8/R3K3 w KQ - 0 1", "castdiag-single-rook"),
]:
    print(f"validate_fen {variant} {pyffish.validate_fen(fen, variant, False)}")
PY
)

if ! echo "${castling_diag_output}" | grep -q "validate_fen castdiag-empty -5"; then
  echo "${castling_diag_output}"
  exit 1
fi

if ! echo "${castling_diag_output}" | grep -q "validate_fen castdiag-wrongpiece -5"; then
  echo "${castling_diag_output}"
  exit 1
fi

if ! echo "${castling_diag_output}" | grep -q "validate_fen castdiag-single-rook -5"; then
  echo "${castling_diag_output}"
  exit 1
fi

if ! echo "${castling_diag_output}" | grep -q "No castling rook on file J for flag J."; then
  echo "${castling_diag_output}"
  exit 1
fi

if ! echo "${castling_diag_output}" | grep -q "Flag J refers to file J, but that square does not contain a WHITE castling rook."; then
  echo "${castling_diag_output}"
  exit 1
fi

if ! echo "${castling_diag_output}" | grep -q "No castling rook for flag K on castling rank 1."; then
  echo "${castling_diag_output}"
  exit 1
fi

echo "parser regression tests passed"
