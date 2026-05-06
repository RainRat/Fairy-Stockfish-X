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

verify_warning "wallingRule and gating features (seirawanGating, potions, gating, gatingPieceAfter) are incompatible." "seirawanGating check"
verify_warning "wallingRule and gating features (seirawanGating, potions, gating, gatingPieceAfter) are incompatible." "potions check"
verify_warning "Variant 'parse-error-empty-fields' has invalid configuration. Skipping." "empty piece-int map rejection"
verify_warning "Variant 'parse-error-empty-piece-map' has invalid configuration. Skipping." "empty piece-type map rejection"
verify_warning "Variant 'parse-error-empty-drop-map' has invalid configuration. Skipping." "empty drop-piece map rejection"
verify_warning "hostageExchange - Empty value is not allowed." "empty hostageExchange rejection"
verify_warning "Variant 'parse-error-empty-hostage' has invalid configuration. Skipping." "empty hostageExchange variant rejection"
verify_warning "castling - Invalid value - garbage for type bool" "castling trailing garbage rejection"
verify_warning "castlingRookPiece - Deprecated option might be removed in future version." "legacy castling rook warning"
verify_warning "maxRank - Invalid value z for type Rank" "invalid maxRank rejection"
verify_warning "rook - Invalid letter: r" "invalid piece token rejection"
verify_warning "Variant 'invalid-piece-token-garbage' has invalid configuration. Skipping." "invalid piece token variant rejection"
verify_warning "promotionPieceTypes - Invalid syntax." "ambiguous file-piece syntax rejection"
verify_warning "Variant 'promotion-by-file-spaces-extended' has invalid configuration. Skipping." "ambiguous file-piece variant rejection"
verify_warning "promotionLimit - Invalid negative value." "negative promotionLimit rejection"
verify_warning "Variant 'negative-promotion-limit' has invalid configuration. Skipping." "negative promotionLimit variant rejection"
verify_warning "multimoves - Invalid non-positive value." "invalid multimoves rejection"
verify_warning "Variant 'invalid-multimoves' has invalid configuration. Skipping." "invalid multimoves variant rejection"
verify_warning "hostageExchange - Invalid hostage piece type in: q:!" "invalid hostageExchange rejection"
verify_warning "captureForbidden - Invalid mapping token: bad" "invalid captureForbidden rejection"
verify_warning "Variant 'hostage-exchange-invalid' has invalid configuration. Skipping." "hostageExchange invalid variant rejection"
verify_warning "Variant 'capture-forbidden-invalid' has invalid configuration. Skipping." "captureForbidden invalid variant rejection"
verify_warning "wallingRule=duck and petrifyOnCaptureTypes are incompatible." "petrify check"
verify_warning "pieceDrops and any walling are incompatible." "freeDrops check"
verify_warning "falcon looks like a custom piece definition. Use customPieceN = a:W for new custom pieces." "named custom piece hint"
verify_warning "Wrapped boards do not support connect/collinear win conditions." "wrapped connect rejection"
verify_warning "Wrapped boards do not support x/z rider modifiers in customPiece1." "toroidal x/z rejection"
verify_warning "Castling destination is adjacent to castlingKingFile; some GUIs/protocols may not distinguish castling from a normal king move." "adjacent castling warning"
verify_warning "removeConnectN is incompatible with connection win conditions." "removeConnectN connect rejection"
verify_warning "removeConnectN is incompatible with (pseudo/anti-)royal pieces." "removeConnectN royal rejection"
verify_warning "Hex boards do not support square weak-connection drop rules." "hex weak-link rejection"

if printf '%s\n' "${check_output}" | grep -qF "Variant 'trailing-rank-space' has invalid configuration. Skipping."; then
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

piecegroup_dash_output=$(cat <<CMDS | "${ENGINE}" 2>&1
uci
setoption name VariantPath value ${tmp_ini}
setoption name UCI_Variant value piecegroup-dash-child
position startpos
go perft 1
quit
CMDS
)

if ! echo "${piecegroup_dash_output}" | grep -q "a7a8q:"; then
  echo "${piecegroup_dash_output}"
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
