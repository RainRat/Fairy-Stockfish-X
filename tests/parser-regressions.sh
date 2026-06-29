#!/bin/bash

set -euo pipefail

error() {
  echo "parser regression test failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

ENGINE=$(default_engine "${1:-}")

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

[wall-or-move-arrow:chess]
wallingRule = arrow
wallOrMove = true

[toroidal-connect:chess]
toroidal = true
connectN = 4

[toroidal-connect-nxn:chess]
toroidal = true
connectNxN = 2

[toroidal-pushing:chess]
toroidal = true
pushingStrength = q:1

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

[invalid-connect-group:chess]
connectGroup = -2

[invalid-bool-retain:chess]
king = -
potions = true

[initial-capture-betza:chess]
customPiece1 = a:ciW

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

[wrapped-connect-group:fairy]
maxRank = 1
maxFile = d
toroidal = true
pieceToCharTable = -
king = -
customPiece1 = s:m
connectPieceTypes = s
connectGroup = 3
connectHorizontal = true
connectVertical = false
connectDiagonal = false
nMoveRule = 0
startFen = S1SS w - - 0 1

[wrapped-connect-nxn:fairy]
maxRank = 3
maxFile = c
toroidal = true
pieceToCharTable = -
king = -
customPiece1 = s:m
connectNxN = 2
connectPieceTypes = s
connectHorizontal = true
connectVertical = true
connectDiagonal = true
nMoveRule = 0
startFen = S1S/3/S1S w - - 0 1

[wrapped-remove-connect:fairy]
maxRank = 1
maxFile = d
toroidal = true
pieceToCharTable = -
king = -
customPiece1 = s:m
pieceDrops = true
immobile = s
removeConnectN = 3
removeConnectNByType = true
startFen = S2S[S] w - - 0 1

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

printf '%s
' '[trailing-rank-space:chess]' 'maxRank = 8 ' >> "${tmp_ini}"

echo "parser regression tests started"

bad_betza_ini=$(mktemp)
bad_hopper_brace_ini=$(mktemp)
bad_rider_range_ini=$(mktemp)
bad_rank_wildcard_ini=$(mktemp)
twochar_hint_ini=$(mktemp)
bad_hopper_type_ini=$(mktemp)
bad_hopper_numeric_ini=$(mktemp)
capture_allowed_only_ini=$(mktemp)
bad_rider_range_val_ini=$(mktemp)
bad_tuple_atom_ini=$(mktemp)
unsupported_bent_rose_modifier_ini=$(mktemp)
bad_ini_syntax_option_ini=$(mktemp)
bad_check_counting_ini=$(mktemp)
bad_hopper_minmax_ini=$(mktemp)
bad_piece_value_ini=$(mktemp)
unknown_option_ini=$(mktemp)
bad_royal_betza_ini=$(mktemp)
bad_unmatched_closer_ini=$(mktemp)
royal_blast_ini=$(mktemp)
nonking_ini=$(mktemp)
wrapped_support_ini=$(mktemp)
trap 'rm -f "${tmp_ini}" "${bad_betza_ini}" "${bad_hopper_brace_ini}" "${bad_rider_range_ini}" "${bad_rank_wildcard_ini}" "${twochar_hint_ini}" "${bad_hopper_type_ini}" "${bad_hopper_numeric_ini}" "${capture_allowed_only_ini}" "${bad_rider_range_val_ini}" "${bad_tuple_atom_ini}" "${unsupported_bent_rose_modifier_ini}" "${bad_ini_syntax_option_ini}" "${bad_check_counting_ini}" "${bad_hopper_minmax_ini}" "${bad_piece_value_ini}" "${unknown_option_ini}" "${bad_royal_betza_ini}" "${bad_unmatched_closer_ini}" "${royal_blast_ini}" "${nonking_ini}" "${wrapped_support_ini}"' EXIT

cat > "${bad_betza_ini}" <<'INI'
[custom-piece-missing-betza:chess]
customPiece1 = a:
INI

cat > "${bad_hopper_brace_ini}" <<'INI'
[hopper-missing-brace:chess]
customPiece1 = a:R{hurdles: 1,1
INI

cat > "${bad_rider_range_ini}" <<'INI'
[rider-missing-bracket:chess]
customPiece1 = a:R[3-
INI

cat > "${bad_rank_wildcard_ini}" <<'INI'
[piecegroup-rank-wildcard-reject:chess]
promotionRegion = P(a1*)
INI

cat > "${twochar_hint_ini}" <<'INI'
[named-custom-piece-hint-twochar:chess]
falcon = P':W
INI

cat > "${bad_hopper_type_ini}" <<'INI'
[bad-hopper-type:chess]
customPiece1 = d:{hurdles: 1,1; pre: 1,*; post: 1,1; hurdle_types: enemy,bogus}R
INI

cat > "${bad_hopper_numeric_ini}" <<'INI'
[bad-hopper-numeric:chess]
customPiece1 = d:{hurdles: abc,1; pre: 1,*}R
INI

cat > "${capture_allowed_only_ini}" <<'INI'
[capture-allowed-only:chess]
king = -
checking = false
captureAllowed = *:p
startFen = 8/3p4/8/8/3Q2n1/8/8/8 w - - 0 1
INI

cat > "${bad_rider_range_val_ini}" <<'INI'
[rider-range-val-1:chess]
customPiece1 = a:N[3-5]

[rider-range-val-2:chess]
customPiece2 = b:R[-3]

[rider-range-val-3:chess]
customPiece3 = c:R[5-3]
INI

cat > "${bad_tuple_atom_ini}" <<'INI'
[tuple-atom-1:chess]
customPiece1 = a:(1)

[tuple-atom-2:chess]
customPiece2 = b:(x,2)

[tuple-atom-3:chess]
customPiece3 = c:(0,0)

[tuple-atom-4:chess]
customPiece4 = d:(999,1)
INI

cat > "${unsupported_bent_rose_modifier_ini}" <<'INI'
[unsupported-bent-1:chess]
customPiece1 = a:jO

[unsupported-rose-1:chess]
customPiece2 = b:j@
INI

cat > "${bad_ini_syntax_option_ini}" <<'INI'
[bad-ini-syntax:chess]
badOptionWithoutEquals
INI

cat > "${bad_check_counting_ini}" <<'INI'
[bad-check-counting:chess]
checking = false
checkCounting = true
startFen = 4k3/8/8/8/8/8/8/4K3 w - - 0 1
INI

cat > "${bad_hopper_minmax_ini}" <<'INI'
[bad-hopper-minmax:chess]
customPiece1 = a:{hurdles:3,1}R
INI

cat > "${bad_piece_value_ini}" <<'INI'
[bad-piece-value:chess]
pieceValueMg = p:not_an_int
INI

cat > "${unknown_option_ini}" <<'INI'
[unknown-option:chess]
pieceDrop = true
INI

cat > "${bad_royal_betza_ini}" <<'INI'
[bad-royal-betza:chess]
king = k:R[
INI

cat > "${bad_unmatched_closer_ini}" <<'INI'
[bad-hopper-close:chess]
customPiece1 = a:}

[bad-rider-close:chess]
customPiece1 = a:]
INI

cat > "${royal_blast_ini}" <<'INI'
[royal-blast-allowed:chess]
checking = true
blastOnCapture = true
castling = false
startFen = 4r2k/8/8/8/8/4p3/3Qp3/4K3 w - - 0 1
INI

check_output=$("${ENGINE}" check "${bad_betza_ini}" 2>&1 || true)
assert_contains "${check_output}" "customPiece1 - Missing Betza move notation"

initial_capture_output=$("${ENGINE}" check "${tmp_ini}" 2>&1 || true)
assert_contains "${initial_capture_output}" "Initial capture Betza moves are not supported in 'ciW'"
assert_contains_literal "${initial_capture_output}" "connectGroup must be -1, 0, or a positive group size."

wrapped_support_output=$("${ENGINE}" check "${wrapped_support_ini}" 2>&1 || true)
assert_not_contains_literal "${wrapped_support_output}" "Wrapped boards do not support connect3D/connect4D win conditions."

check_output=$("${ENGINE}" check "${bad_hopper_brace_ini}" 2>&1 || true)
assert_contains "${check_output}" "customPiece1 - Invalid Betza hopper parameters in 'R{hurdles: 1,1': missing closing '}'."

check_output=$("${ENGINE}" check "${bad_rider_range_ini}" 2>&1 || true)
assert_contains "${check_output}" "customPiece1 - Invalid Betza rider range in 'R\[3-': missing closing '\]'."

check_output=$("${ENGINE}" check "${bad_rank_wildcard_ini}" 2>&1 || true)
assert_contains "${check_output}" "Illegal rank character: \*"

check_output=$("${ENGINE}" check "${twochar_hint_ini}" 2>&1 || true)
assert_contains "${check_output}" "falcon looks like a custom piece definition. Use customPieceN = P':W for new custom pieces."

bad_hopper_type_output=$(run_uci "$ENGINE" "$bad_hopper_type_ini" "bad-hopper-type" </dev/null 2>&1 || true)
assert_contains "${bad_hopper_type_output}" "unknown variant 'bad-hopper-type'; keeping 'chess'"

bad_hopper_numeric_output=$(run_uci "$ENGINE" "$bad_hopper_numeric_ini" "bad-hopper-numeric" </dev/null 2>&1 || true)
assert_contains "${bad_hopper_numeric_output}" "Invalid numeric value in Betza hopper parameters: 'abc,1'"
assert_contains "${bad_hopper_numeric_output}" "unknown variant 'bad-hopper-numeric'; keeping 'chess'"

wall_or_move_arrow_output=$(run_uci "$ENGINE" "$tmp_ini" "wall-or-move-arrow" </dev/null 2>&1 || true)
assert_contains "${wall_or_move_arrow_output}" "unknown variant 'wall-or-move-arrow'; keeping 'chess'"

toroidal_pushing_output=$(run_uci "$ENGINE" "$tmp_ini" "toroidal-pushing" </dev/null 2>&1 || true)
assert_contains "${toroidal_pushing_output}" "^info string variant toroidal-pushing "
assert_not_contains "${toroidal_pushing_output}" "invalid configuration"

toroidal_connect_nxn_output=$(run_uci "$ENGINE" "$tmp_ini" "toroidal-connect-nxn" </dev/null 2>&1 || true)
assert_contains "${toroidal_connect_nxn_output}" "^info string variant toroidal-connect-nxn "
assert_not_contains "${toroidal_connect_nxn_output}" "invalid configuration"

invalid_connect_group_output=$(run_uci "$ENGINE" "$tmp_ini" "invalid-connect-group" </dev/null 2>&1 || true)
assert_contains "${invalid_connect_group_output}" "unknown variant 'invalid-connect-group'; keeping 'chess'"

royal_blast_output=$(run_uci "${ENGINE}" "${royal_blast_ini}" "royal-blast-allowed" <<'EOF'
d
EOF
)
assert_contains "${royal_blast_output}" "^info string variant royal-blast-allowed "
assert_not_contains "${royal_blast_output}" "invalid configuration"

capture_allowed_only_output=$(run_uci "$ENGINE" "$capture_allowed_only_ini" "capture-allowed-only" <<EOF
position fen 8/3p4/8/8/3Q2n1/8/8/8 w - - 0 1
go perft 1
EOF
)
assert_contains "${capture_allowed_only_output}" "d4d7:"
assert_not_contains "${capture_allowed_only_output}" "d4g4:"

bad_rider_range_val_output1=$(run_uci "$ENGINE" "$bad_rider_range_val_ini" "rider-range-val-1" </dev/null 2>&1 || true)
assert_contains "${bad_rider_range_val_output1}" "unknown variant 'rider-range-val-1'; keeping 'chess'"

bad_rider_range_val_output2=$(run_uci "$ENGINE" "$bad_rider_range_val_ini" "rider-range-val-2" </dev/null 2>&1 || true)
assert_contains "${bad_rider_range_val_output2}" "unknown variant 'rider-range-val-2'; keeping 'chess'"

bad_rider_range_val_output3=$(run_uci "$ENGINE" "$bad_rider_range_val_ini" "rider-range-val-3" </dev/null 2>&1 || true)
assert_contains "${bad_rider_range_val_output3}" "unknown variant 'rider-range-val-3'; keeping 'chess'"

bad_tuple_atom_output1=$(run_uci "$ENGINE" "$bad_tuple_atom_ini" "tuple-atom-1" </dev/null 2>&1 || true)
assert_contains "${bad_tuple_atom_output1}" "unknown variant 'tuple-atom-1'; keeping 'chess'"

bad_tuple_atom_output2=$(run_uci "$ENGINE" "$bad_tuple_atom_ini" "tuple-atom-2" </dev/null 2>&1 || true)
assert_contains "${bad_tuple_atom_output2}" "unknown variant 'tuple-atom-2'; keeping 'chess'"

bad_tuple_atom_output3=$(run_uci "$ENGINE" "$bad_tuple_atom_ini" "tuple-atom-3" </dev/null 2>&1 || true)
assert_contains "${bad_tuple_atom_output3}" "unknown variant 'tuple-atom-3'; keeping 'chess'"

bad_tuple_atom_output4=$(run_uci "$ENGINE" "$bad_tuple_atom_ini" "tuple-atom-4" </dev/null 2>&1 || true)
assert_contains "${bad_tuple_atom_output4}" "unknown variant 'tuple-atom-4'; keeping 'chess'"

unsupported_bent_rose_output1=$(run_uci "$ENGINE" "$unsupported_bent_rose_modifier_ini" "unsupported-bent-1" </dev/null 2>&1 || true)
assert_contains "${unsupported_bent_rose_output1}" "unknown variant 'unsupported-bent-1'; keeping 'chess'"

unsupported_bent_rose_output2=$(run_uci "$ENGINE" "$unsupported_bent_rose_modifier_ini" "unsupported-rose-1" </dev/null 2>&1 || true)
assert_contains "${unsupported_bent_rose_output2}" "unknown variant 'unsupported-rose-1'; keeping 'chess'"

bad_ini_syntax_output=$("${ENGINE}" check "${bad_ini_syntax_option_ini}" 2>&1 || true)
assert_contains "${bad_ini_syntax_output}" "Invalid syntax: 'badOptionWithoutEquals'"
assert_contains_literal "${bad_ini_syntax_output}" "Variant 'bad-ini-syntax' has invalid configuration. Skipping."
bad_ini_syntax_uci_output=$(run_uci "$ENGINE" "$bad_ini_syntax_option_ini" "bad-ini-syntax" </dev/null 2>&1 || true)
assert_contains "${bad_ini_syntax_uci_output}" "unknown variant 'bad-ini-syntax'; keeping 'chess'"

bad_check_counting_output=$("${ENGINE}" check "${bad_check_counting_ini}" 2>&1 || true)
assert_contains_literal "${bad_check_counting_output}" "checkCounting=true requires checking=true."
assert_contains_literal "${bad_check_counting_output}" "Variant 'bad-check-counting' has invalid configuration. Skipping."

bad_hopper_minmax_output=$(run_uci "$ENGINE" "$bad_hopper_minmax_ini" "bad-hopper-minmax" </dev/null 2>&1 || true)
assert_contains_literal "${bad_hopper_minmax_output}" "Invalid hopper range (min > max)"
assert_contains "${bad_hopper_minmax_output}" "unknown variant 'bad-hopper-minmax'; keeping 'chess'"

bad_piece_value_check_output=$("${ENGINE}" check "${bad_piece_value_ini}" 2>&1 || true)
assert_contains "${bad_piece_value_check_output}" "pieceValueMg - Invalid syntax."
bad_piece_value_uci_output=$(run_uci "$ENGINE" "$bad_piece_value_ini" "bad-piece-value" </dev/null 2>&1 || true)
assert_contains "${bad_piece_value_uci_output}" "unknown variant 'bad-piece-value'; keeping 'chess'"

unknown_option_check_output=$("${ENGINE}" check "${unknown_option_ini}" 2>&1 || true)
assert_contains_literal "${unknown_option_check_output}" "Unknown option ignored: pieceDrop"
assert_not_contains_literal "${unknown_option_check_output}" "has invalid configuration. Skipping."
unknown_option_uci_output=$(run_uci "$ENGINE" "$unknown_option_ini" "unknown-option" </dev/null 2>&1 || true)
assert_contains_literal "${unknown_option_uci_output}" "Unknown option ignored: pieceDrop"
assert_not_contains "${unknown_option_uci_output}" "unknown variant 'unknown-option'"

bad_royal_betza_output=$(run_uci "$ENGINE" "$bad_royal_betza_ini" "bad-royal-betza" </dev/null 2>&1 || true)
assert_contains "${bad_royal_betza_output}" "king - Invalid Betza rider range in 'R\\[': missing closing '\\]'."
assert_contains "${bad_royal_betza_output}" "unknown variant 'bad-royal-betza'; keeping 'chess'"

bad_unmatched_closer_output=$("${ENGINE}" check "${bad_unmatched_closer_ini}" 2>&1 || true)
assert_contains_literal "${bad_unmatched_closer_output}" "customPiece1 - Invalid Betza hopper parameters in '}': missing opening '{'."
assert_contains_literal "${bad_unmatched_closer_output}" "customPiece1 - Invalid Betza rider range in ']': missing opening '['."

two_boards_output=$(python3 - <<'PY' 2>&1
import sys
import pyffish

pyffish.load_variant_config("[x:chess]\ntwoBoards = true" + "   \n")
print("two_boards_trailing_space_bool", pyffish.two_boards("x"))
PY
)
assert_contains "${two_boards_output}" "two_boards_trailing_space_bool True"

capture_type_output=$(python3 - <<'PY' 2>&1
import pyffish

pyffish.load_variant_config("[x:chess]\ncaptureType = hand" + "   \n")
print("capture_type_trailing_space_enum", pyffish.captures_to_hand("x"))
PY
)
assert_contains "${capture_type_output}" "capture_type_trailing_space_enum True"

header_trim_output=$(python3 - <<'PY' 2>&1
import pyffish

pyffish.load_variant_config("""
[ spaced-parent : chess ]
twoBoards = true

[ spaced-child : spaced-parent ]
""")
print("trimmed_header_inheritance", pyffish.two_boards("spaced-child"))
PY
)
assert_contains "${header_trim_output}" "trimmed_header_inheritance True"

priority_drop_clear_output=$(python3 - <<'PY' 2>&1
import pyffish

pyffish.load_variant_config("""
[priority-clear-parent:chess]
pieceDrops = true
priorityDropTypes = p
startFen = 8/8/8/8/8/8/8/4K2k[PN] w - - 0 1

[priority-clear-child:priority-clear-parent]
priorityDropTypes = -
""")
moves = pyffish.legal_moves("priority-clear-child", pyffish.start_fen("priority-clear-child"), [])
print("priority_drop_clear_allows_knight", "N@d4" in moves)
PY
)
assert_contains "${priority_drop_clear_output}" "priority_drop_clear_allows_knight True"

cat > "${nonking_ini}" <<'INI'
[nonking-inline-betza:chess]
rook = r:R3
INI

tuple_output=$(run_uci "$ENGINE" "$tmp_ini" "tuple-nonsquare" <<EOF
position startpos
go perft 1
EOF
)
assert_not_contains "${tuple_output}" "No piece char found for custom piece"

promotion_file_output=$(run_uci "$ENGINE" "$tmp_ini" "promotion-by-file-inherit" <<EOF
position startpos
go perft 1
EOF
)
assert_contains "${promotion_file_output}" "b7b8r:"
assert_not_contains "${promotion_file_output}" "b7b8q:"
assert_not_contains "${promotion_file_output}" "b7b8n:"

promotion_spaces_output=$(run_uci "$ENGINE" "$tmp_ini" "promotion-by-file-spaces" <<EOF
position startpos
go perft 1
EOF
)
assert_contains "${promotion_spaces_output}" "b7b8r:"

terminal_output=$(run_uci "$ENGINE" "" "" <<EOF
position fen 7k/5Q2/7K/8/8/8/8/8 b - - 0 1
go depth 1
EOF
)
assert_contains_literal "${terminal_output}" "bestmove (none)"

bench_output=$("${ENGINE}" bench 16 1 1 default nonsense 2>&1 || true)
assert_contains "${bench_output}" "Nodes searched  : "

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

assert_contains "${castling_diag_output}" "validate_fen castdiag-empty -5"
assert_contains "${castling_diag_output}" "validate_fen castdiag-wrongpiece -5"
assert_contains "${castling_diag_output}" "validate_fen castdiag-single-rook -5"
assert_contains "${castling_diag_output}" "No castling rook on file J for flag J."
assert_contains "${castling_diag_output}" "Flag J refers to file J, but that square does not contain a WHITE castling rook."
assert_contains "${castling_diag_output}" "No castling rook for flag K on castling rank 1."

counter_diag_output=$(python3 - <<'PY' 2>&1
import pyffish

pyffish.load_variant_config(
    """
[counterdiag:gothic]
maxRank = 8
maxFile = 8
checkCounting = true
startFen = 4k3/8/8/8/8/8/8/4K3 w - - 0 1
"""
)

for fen in [
    ("4k3/8/8/8/8/8/8/4K3 w - - x 1 a0a0", "half move"),
    ("4k3/8/8/8/8/8/8/4K3 w - - 0 x a0a0", "move"),
]:
    print(f"{fen[1]} {pyffish.validate_fen(fen[0], 'counterdiag', False)}")
PY
)

assert_contains_literal "${counter_diag_output}" "half move -2"
assert_contains_literal "${counter_diag_output}" "move -1"
assert_contains_literal "${counter_diag_output}" "Invalid half move counter: 'x'."
assert_contains_literal "${counter_diag_output}" "Invalid move counter: 'x'."

help_output=$(run_uci "$ENGINE" "$(default_variants)" chess <<EOF
help
EOF
)
assert_contains_literal "${help_output}" "export_net [file]"

variants_lower=$(run_uci "$ENGINE" "$(default_variants)" chess <<EOF
variants sh
EOF
)
variants_upper=$(run_uci "$ENGINE" "$(default_variants)" chess <<EOF
variants SH
EOF
)

if [[ "${variants_lower}" != "${variants_upper}" ]]; then
    echo "expected variants filter to be case-insensitive" >&2
    echo "--- variants sh ---" >&2
    printf '%s\n' "${variants_lower}" >&2
    echo "--- variants SH ---" >&2
    printf '%s\n' "${variants_upper}" >&2
    exit 1
fi

echo "parser regression tests passed"
