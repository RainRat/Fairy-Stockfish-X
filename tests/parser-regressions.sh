#!/bin/bash

set -euo pipefail

error() {
  echo "parser regression test failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
ROOT_DIR=$(cd "${SCRIPT_DIR}/.." && pwd)
ENGINE=${1:-${SCRIPT_DIR}/../src/stockfish}

cd "${ROOT_DIR}"

tmp_ini=$(mktemp)
trap 'rm -f "${tmp_ini}"' EXIT

spaces='   '
cat > "${tmp_ini}" <<INI
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

[parse-error-empty-piece-map:chess]
promotionPieceTypes =

[parse-error-empty-drop-map:chess]
dropPieceTypes =

[parse-error-empty-hostage:chess]
hostageExchange =

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

[promotion-by-file-spaces-extended:chess]
promotionPieceTypes = a:q r b:n
startFen = 8/1P6/8/8/8/8/8/4k2K w - - 0 1

[promotion-pawn-clear:chess]
promotionPawnTypes = -

[invalid-piece-token-garbage:chess]
rook = rxyz

[piecegroup-dash-parent:chess]
promotionRegion = a8
promotionPieceTypes = q
startFen = 8/P7/8/8/8/8/8/4k2K w - - 0 1

[piecegroup-dash-child:piecegroup-dash-parent]
promotionRegion = - garbage

[negative-promotion-limit:chess]
promotionLimit = p:-1

[invalid-multimoves:chess]
multimoves = 1 0

[invalid-bool-retain:chess]
king = -
potions = true

[invalid-bool-retain-child:invalid-bool-retain]
potions = maybe
wallingRule = duck

[castling-trailing-garbage:chess]
castling = - garbage

[two-boards-trailing-space-bool:chess]
twoBoards = true${spaces}

[capture-type-trailing-space-enum:chess]
captureType = hand${spaces}
startFen = r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1

[legacy-castling-rook-piece:chess]
castlingRookPiece = r

[invalid-maxrank:chess]
maxRank = z

[hostage-exchange-invalid:chess]
hostageExchange = p:p q:!

[capture-forbidden-invalid:chess]
captureForbidden = p:q q:r bad

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

printf '%s\n' '[trailing-rank-space:chess]' 'maxRank = 8 ' >> "${tmp_ini}"

echo "parser regression tests started"

bad_betza_ini=$(mktemp)
bad_rank_wildcard_ini=$(mktemp)
twochar_hint_ini=$(mktemp)
trap 'rm -f "${tmp_ini}" "${bad_betza_ini}" "${bad_rank_wildcard_ini}" "${twochar_hint_ini}"' EXIT

cat > "${bad_betza_ini}" <<'INI'
[custom-piece-missing-betza:chess]
customPiece1 = a
INI

cat > "${bad_rank_wildcard_ini}" <<'INI'
[piecegroup-rank-wildcard-reject:chess]
promotionRegion = P(a1*)
INI

cat > "${twochar_hint_ini}" <<'INI'
[named-custom-piece-hint-twochar:chess]
falcon = P':W
INI

check_output=$("${ENGINE}" check "${bad_betza_ini}" 2>&1 || true)
if ! echo "${check_output}" | grep -q "customPiece1 - Missing Betza move notation"; then
  echo "${check_output}"
  exit 1
fi

check_output=$("${ENGINE}" check "${bad_rank_wildcard_ini}" 2>&1 || true)
if ! echo "${check_output}" | grep -q "Illegal rank character: \*"; then
  echo "${check_output}"
  exit 1
fi

check_output=$("${ENGINE}" check "${twochar_hint_ini}" 2>&1 || true)
if ! echo "${check_output}" | grep -q "falcon looks like a custom piece definition. Use customPieceN = P':W for new custom pieces."; then
  echo "${check_output}"
  exit 1
fi

two_boards_output=$(python3 - <<'PY' 2>&1
import sys
import pyffish

pyffish.load_variant_config("[x:chess]\ntwoBoards = true" + "   \n")
print("two_boards_trailing_space_bool", pyffish.two_boards("x"))
PY
)

if ! printf '%s\n' "${two_boards_output}" | grep -qF "two_boards_trailing_space_bool True"; then
  echo "${two_boards_output}"
  exit 1
fi

capture_type_output=$(python3 - <<'PY' 2>&1
import pyffish

pyffish.load_variant_config("[x:chess]\ncaptureType = hand" + "   \n")
print("capture_type_trailing_space_enum", pyffish.captures_to_hand("x"))
PY
)

if ! printf '%s\n' "${capture_type_output}" | grep -qF "capture_type_trailing_space_enum True"; then
  echo "${capture_type_output}"
  exit 1
fi

nonking_ini=$(mktemp)
trap 'rm -f "${tmp_ini}" "${nonking_ini}"' EXIT
cat > "${nonking_ini}" <<'INI'
[nonking-inline-betza:chess]
rook = r:R3
INI

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
[castdiag-empty:gothic]
castling = true
castlingKingFile = f
castlingKingsideFile = i
castlingQueensideFile = c
castlingRookKingsideFile = j
castlingRookQueensideFile = b
startFen = 10/10/10/10/10/10/10/1R3K3R w JQ - 0 1

[castdiag-wrongpiece:gothic]
castling = true
castlingKingFile = f
castlingKingsideFile = i
castlingQueensideFile = c
castlingRookKingsideFile = j
castlingRookQueensideFile = b
startFen = 10/10/10/10/10/10/10/1R3K3R w JQ - 0 1

[castdiag-single-rook:gothic]
castling = true
startFen = 10/10/10/10/10/10/10/1R3K3R w JQ - 0 1
"""
)

for fen, variant in [
    ("10/10/10/10/10/10/10/1R3K2R1 w JQ - 0 1", "castdiag-empty"),
    ("10/10/10/10/10/10/10/1R3K3N w JQ - 0 1", "castdiag-wrongpiece"),
    ("10/10/10/10/10/10/10/1R3K6 w KQ - 0 1", "castdiag-single-rook"),
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
