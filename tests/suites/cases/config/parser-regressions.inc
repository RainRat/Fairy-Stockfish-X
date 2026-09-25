#!/bin/bash

set -euo pipefail

error() {
  echo "parser regression test failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/../../../../tests/lib/uci.sh"

ENGINE=$(default_engine "${1:-}")

cd "${ROOT_DIR}"

tmp_ini=$(mktemp)
fsx_add_exit_cleanup 'rm -f "${tmp_ini}"'

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

[parse-error-empty-piece-set:chess]
promotionPieceTypes =

[parse-error-empty-drop-map:chess]
dropPieceTypes =

[parse-error-empty-hostage:chess]
hostageExchange =

[named-custom-piece-hint:chess]
falcon = a:W

[nnue-alias:chess]
nnueAlias = nn-

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

[promotion-by-result-piece:chess]
promotionPieceTypes = q r n
promotionRegionWhite = Q(a8);R(b8);*(*8)
startFen = 4k3/PP6/8/8/8/8/8/7K w - - 0 1

[promotion-by-result-piece-typed-only:chess]
promotionPieceTypes = q r
promotionRegionWhite = Q(a8)
startFen = 4k3/P7/8/8/8/8/8/7K w - - 0 1

[promotion-region-bare-bitboard:chess]
promotionPieceTypes = q r
promotionRegionWhite = *8
startFen = 4k3/1P6/8/8/8/8/8/7K w - - 0 1

[promotion-pawn-clear:chess]
promotionPawnTypes = -

[invalid-piece-token-garbage:chess]
rook = rxyz

[invalid-piece-removal-trailing:chess]
customPiece1 = - trailing

[invalid-king-removal-trailing:chess]
king = - trailing

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

[overflow-multimoves:chess]
multimoves = 2147483647

[overflow-multimove-cycle:chess]
multimoves = 1073741824 1073741824

[negative-drop-no-doubled-count:chess]
dropNoDoubledCount = -1

[negative-potion-cooldown:chess]
freezeCooldown = -1

[overflow-potion-cooldown:chess]
jumpCooldown = 65536

[invalid-nfold-negative:chess]
nFoldRule = -1

[invalid-nfold-immediate-negative:chess]
nFoldRuleImmediate = -1

[invalid-remove-connect-n:chess]
removeConnectN = -1

[invalid-connect-group:chess]
connectGroup = -2

[invalid-nmove-immediate-negative:chess]
nMoveRuleImmediate = -1

[invalid-nmove-immediate-overflow:chess]
nMoveRuleImmediate = 1073741825

[invalid-nmove-negative:chess]
nMoveRule = -1

[invalid-nmove-overflow:chess]
nMoveRule = 1073741825

[invalid-connect-n-too-large:chess]
connectN = 9

[invalid-connect-nxn-too-large:chess]
connectNxN = 9

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

[capture-forbidden-empty:chess]
captureForbidden =

[capture-allowed-empty:chess]
captureAllowed =

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

[bad-laser-piece:fairy]
laserGame = true
laser_notapiece = R/D/D/L

[bad-laser-faces:fairy]
laserGame = true
laser_p = R/D/D/L/S

[bad-portal-fallback:fairy]
laserGame = true
laserPortalFallback = I

[bad-static-manual-laser:fairy]
laserGame = true
laserAutoFire = false
laserEmitters = white@a1:0

[bad-selected-static-laser:fairy]
laserGame = true
laserFireSelectedEmitter = true
laserEmitters = white@a1:0

[bad-orientation-count-zero:fairy]
orientedPieceTypes = p
orientationCounts = p:0

[bad-orientation-count-malformed:fairy]
orientedPieceTypes = p
orientationCounts = p

[bad-stacking-piece:fairy]
stackedPieceType = p:?

[bad-laser-square:fairy]
laserGame = true
laserEmitters = white@a10x:0

[bad-laser-empty-emitter:fairy]
laserGame = true
laserEmitters = white@a1:0,   ,black@a8:2

[bad-laser-huge-rank:fairy]
laserGame = true
laserEmitters = white@a999999999999999999999999999999:0

[bad-laser-huge-direction:fairy]
laserGame = true
laserEmitters = white@a1:999999999999999999999999999999

[bad-huge-orientation-count:fairy]
customPiece1 = a:K
orientedPieceTypes = a
orientationCounts = a:999999999999999999999999999999

[laser-rank10:fairy]
laserGame = true
maxFile = a
maxRank = 10
pieceToCharTable = -
king = -
checking = false
castling = false
startFen = 1/1/1/1/1/1/1/1/1/1 w - - 0 1
laserEmitters = white@a10:0

[bad-laser-gating:fairy]
laserGame = true
gating = true

[bad-multiple-piece-emitters:fairy]
laserGame = true
customPiece1 = a:K
customPiece2 = b:K
orientedPieceTypes = a b
laserEmitters = piece:a, piece:b

[bad-non-oriented-piece-emitter:fairy]
laserGame = true
laserEmitters = piece:k

[bad-laser-topology:fairy]
laserGame = true
cylindrical = true
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
dropnomate_alias_ini=$(mktemp)
bad_rider_range_val_ini=$(mktemp)
bad_tuple_atom_ini=$(mktemp)
bad_hook_ini=$(mktemp)
unsupported_bent_rose_modifier_ini=$(mktemp)
bad_ini_syntax_option_ini=$(mktemp)
bad_check_counting_ini=$(mktemp)
bad_hopper_minmax_ini=$(mktemp)
bad_hopper_entry_ini=$(mktemp)
bad_piece_value_ini=$(mktemp)
unknown_option_ini=$(mktemp)
bad_royal_betza_ini=$(mktemp)
bad_unmatched_closer_ini=$(mktemp)
royal_blast_ini=$(mktemp)
nonking_ini=$(mktemp)
wrapped_support_ini=$(mktemp)
fsx_add_exit_cleanup 'rm -f "${tmp_ini}" "${bad_betza_ini}" "${bad_hopper_brace_ini}" "${bad_rider_range_ini}" "${bad_rank_wildcard_ini}" "${twochar_hint_ini}" "${bad_hopper_type_ini}" "${bad_hopper_numeric_ini}" "${capture_allowed_only_ini}" "${dropnomate_alias_ini}" "${bad_rider_range_val_ini}" "${bad_tuple_atom_ini}" "${bad_hook_ini}" "${unsupported_bent_rose_modifier_ini}" "${bad_ini_syntax_option_ini}" "${bad_check_counting_ini}" "${bad_hopper_minmax_ini}" "${bad_hopper_entry_ini}" "${bad_piece_value_ini}" "${unknown_option_ini}" "${bad_royal_betza_ini}" "${bad_unmatched_closer_ini}" "${royal_blast_ini}" "${nonking_ini}" "${wrapped_support_ini}"'

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

[capture-merge-parent:chess]
captureForbidden = p:q p:r

[capture-merge-child:capture-merge-parent]
captureAllowed = p:q

[capcolor-parent:chess]
captureForbiddenWhite = r:n

[capcolor-child:capcolor-parent]

[capallow-white-only:chess]
captureAllowedWhite = r:n

[capcolor-merge-parent:chess]
captureForbiddenWhite = r:n

[capcolor-merge-child:capcolor-merge-parent]
captureForbidden = p:q
INI

cat > "${bad_rider_range_val_ini}" <<'INI'
[rider-range-val-1:chess]
customPiece1 = a:N[3-5]

[rider-range-val-2:chess]
customPiece2 = b:R[-3]

[rider-range-val-3:chess]
customPiece3 = c:R[5-3]

[rider-range-val-4:chess]
customPiece4 = d:R[300-]
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

cat > "${bad_hook_ini}" <<'INI'
[bad-hook-leg:chess]
customPiece1 = h:0
hookMoves = h:X

[bad-hook-geometry:chess]
customPiece1 = h:0
hookMoves = h:sB-R

[bad-hook-limit:chess]
customPiece1 = h:0
hookMoves = h:R-R:3

[bad-hook-no-turn:chess]
customPiece1 = h:0
hookMoves = h:fR-bR

[bad-twostep-empty-list:chess]
customPiece1 = l:0
twoStepMoves = l:,

[bad-twostep-repeated-comma:chess]
customPiece1 = l:0
twoStepMoves = l:N>N,,E>E

[hook-bounded-diagonal-legs:chess]
customPiece1 = h:0
hookMoves = h:B2-R
castling = false

[bad-two-leg-mandatory-demotion:chess]
customPiece1 = h:0
hookMoves = h:R-R
mandatoryPiecePromotion = true
pieceDemotion = true
promotedPieceType = p:h

[bad-hook-wrapped:chess]
king = -
customPiece1 = h:0
hookMoves = h:R-R:2
cylindrical = true
startFen = 8/8/8/8/4H3/8/8/8 w - - 0 1
immobile = -
castling = false
doubleStep = false

[bad-hook-toroidal:chess]
customPiece1 = h:0
hookMoves = h:R-R
toroidal = true

[bad-hook-twoboards:chess]
customPiece1 = h:0
hookMoves = h:R-R:2
twoBoards = true

[hook-twoboards-single:chess]
customPiece1 = h:0
hookMoves = h:R-R
twoBoards = true

[bad-twostep-twoboards:chess]
customPiece1 = l:0
twoStepMoves = l:N>N
twoBoards = true

[hook-parent:chess]
customPiece1 = h:0
castling = false

[bad-inherited-hook-wrapped:hook-parent]
hookMoves = h:R-R
cylindrical = true

[bad-inherited-lion-twoboards:hook-parent]
twoStepMoves = h:N>N
twoBoards = true

[hook-clear-parent:chess]
customPiece1 = h:0
hookMoves = h:R-R

[inherited-hook-cleared:hook-clear-parent]
hookMoves = h:-
twoBoards = true
castling = false

[lion-clear-parent:chess]
customPiece1 = l:0
twoStepMoves = l:N>N

[inherited-lion-cleared:lion-clear-parent]
twoStepMoves = l:-
twoBoards = true
castling = false
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

cat > "${bad_hopper_entry_ini}" <<'INI'
[bad-hopper-empty:chess]
customPiece1 = a:R{}

[bad-hopper-separator:chess]
customPiece1 = b:R{hurdles:1,1;;post:1,1}
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

cat > "${dropnomate_alias_ini}" <<'INI'
[nc-alias-legacy:minishogi]
shogiPawnDropMateIllegal = true
startFen = 2k2/5/2K2/5/1R1R1[P] w - - 0 1

[nc-alias-newkey:minishogi]
dropNoCheckmate = p
startFen = 2k2/5/2K2/5/1R1R1[P] w - - 0 1

[nc-alias-optout:nc-alias-legacy]
shogiPawnDropMateIllegal = false

[nc-alias-newoptout:nc-alias-newkey]
shogiPawnDropMateIllegal = false
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
assert_contains_literal "${initial_capture_output}" "multimoves - Derived cycle exceeds the supported int range."
assert_contains_literal "${initial_capture_output}" "dropNoDoubledCount - Invalid negative value."
assert_contains_literal "${initial_capture_output}" "Potion cooldown must be between 0 and 65535."
assert_contains_literal "${initial_capture_output}" "nFoldRule cannot be negative."
assert_contains_literal "${initial_capture_output}" "nFoldRuleImmediate cannot be negative."
assert_contains_literal "${initial_capture_output}" "removeConnectN - Value must be in range [0,"
assert_contains_literal "${initial_capture_output}" "nMoveRule must be between 0 and 1073741824."
assert_contains_literal "${initial_capture_output}" "nMoveRuleImmediate must be between 0 and 1073741824."
assert_contains_literal "${initial_capture_output}" "connectN must be between 0 and the largest board dimension."
assert_contains_literal "${initial_capture_output}" "connectNxN must be -1, 0, or no larger than the smallest board dimension."
assert_contains_literal "${initial_capture_output}" "laser_notapiece - Unknown piece symbol: notapiece"
assert_contains_literal "${initial_capture_output}" "laser_p - Too many laser outcome faces: expected 4."
assert_contains_literal "${initial_capture_output}" "laserPortalFallback must be a non-portal laser outcome."
assert_contains_literal "${initial_capture_output}" "laserAutoFire = false requires a piece laser emitter."
assert_contains_literal "${initial_capture_output}" "laserFireSelectedEmitter is incompatible with static laser emitters."
assert_contains_literal "${initial_capture_output}" "orientationCounts - Invalid orientation count: 0"
assert_contains_literal "${initial_capture_output}" "orientationCounts - Malformed entry: p"
assert_contains_literal "${initial_capture_output}" "stackedPieceType - Invalid syntax."
assert_contains_literal "${initial_capture_output}" "laserEmitters - Invalid square coordinates: a10x"
assert_contains_literal "${initial_capture_output}" "laserEmitters - Malformed token: "
assert_contains_literal "${initial_capture_output}" "laserEmitters - Invalid square coordinates: a999999999999999999999999999999"
assert_contains_literal "${initial_capture_output}" "laserEmitters - Invalid direction: 999999999999999999999999999999"
assert_contains_literal "${initial_capture_output}" "orientationCounts - Invalid orientation count: 999999999999999999999999999999"
assert_contains_literal "${initial_capture_output}" "laserGame is incompatible with legacy gating"
assert_contains_literal "${initial_capture_output}" "laserEmitters - Multiple piece emitters are not supported."
assert_contains_literal "${initial_capture_output}" "laserEmitters - Piece emitter must be an oriented piece type: k"
assert_contains_literal "${initial_capture_output}" "laserGame is not supported on wrapped or hexagonal boards."
assert_contains_literal "${initial_capture_output}" "customPiece1 - Invalid syntax."
assert_contains_literal "${initial_capture_output}" "king - Invalid syntax."
assert_not_contains_literal "${initial_capture_output}" "Unknown option ignored: nnueAlias"

laser_rank10_output=$(run_uci "$ENGINE" "$tmp_ini" "laser-rank10" </dev/null 2>&1 || true)
assert_contains "${laser_rank10_output}" "^info string variant laser-rank10 "

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

bad_hopper_empty_output=$(run_uci "$ENGINE" "$bad_hopper_entry_ini" "bad-hopper-empty" </dev/null 2>&1 || true)
assert_contains "${bad_hopper_empty_output}" "unknown variant 'bad-hopper-empty'; keeping 'chess'"

bad_hopper_separator_output=$(run_uci "$ENGINE" "$bad_hopper_entry_ini" "bad-hopper-separator" </dev/null 2>&1 || true)
assert_contains "${bad_hopper_separator_output}" "unknown variant 'bad-hopper-separator'; keeping 'chess'"

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

# A child specifying only captureAllowed narrows the parent's forbiddens
# instead of discarding them: e4d5 is re-allowed, e4f5 stays banned, and the
# unrelated c4b6 knight capture (banned under replace semantics) stays legal.
capture_merge_parent_output=$(run_uci "$ENGINE" "$capture_allowed_only_ini" "capture-merge-parent" <<EOF
position fen 4k3/8/1p6/3q1r2/2N1P3/8/8/4K3 w - - 0 1
go perft 1
EOF
)
assert_not_contains "${capture_merge_parent_output}" "e4d5:"
assert_not_contains "${capture_merge_parent_output}" "e4f5:"
assert_contains "${capture_merge_parent_output}" "c4b6:"

capture_merge_child_output=$(run_uci "$ENGINE" "$capture_allowed_only_ini" "capture-merge-child" <<EOF
position fen 4k3/8/1p6/3q1r2/2N1P3/8/8/4K3 w - - 0 1
go perft 1
EOF
)
assert_contains "${capture_merge_child_output}" "e4d5:"
assert_not_contains "${capture_merge_child_output}" "e4f5:"
assert_contains "${capture_merge_child_output}" "c4b6:"

# A lone captureAllowedWhite on an empty color map is an allowlist for white:
# rooks take knights but nothing else.
capallow_white_output=$(run_uci "$ENGINE" "$capture_allowed_only_ini" "capallow-white-only" <<EOF
position fen 4k3/8/8/8/8/8/b7/Rn2K3 w - - 0 1
go perft 1
EOF
)
assert_contains "${capallow_white_output}" "a1b1:"
assert_not_contains "${capallow_white_output}" "a1a2:"

# A keyless child inherits the parent's color-specific map: the white
# rook-knight ban holds in both parent and child.
for variant in capcolor-parent capcolor-child; do
  capcolor_output=$(run_uci "$ENGINE" "$capture_allowed_only_ini" "$variant" <<EOF
position fen 4k3/8/8/8/8/8/8/Rn2K3 w - - 0 1
go perft 1
EOF
)
  assert_not_contains "${capcolor_output}" "a1b1:"
  assert_contains "${capcolor_output}" "a1a2:"
done

# A child global key composes onto the inherited color map instead of
# replacing it: the child bans pawn-takes-queen globally while keeping the
# inherited rook-takes-knight ban for white (which the parent lacks).
capcolor_merge_parent_output=$(run_uci "$ENGINE" "$capture_allowed_only_ini" "capcolor-merge-parent" <<EOF
position fen 4k3/8/8/3q4/4P3/8/8/Rn2K3 w - - 0 1
go perft 1
EOF
)
assert_not_contains "${capcolor_merge_parent_output}" "a1b1:"
assert_contains "${capcolor_merge_parent_output}" "e4d5:"
assert_contains "${capcolor_merge_parent_output}" "e4e5:"

capcolor_merge_child_output=$(run_uci "$ENGINE" "$capture_allowed_only_ini" "capcolor-merge-child" <<EOF
position fen 4k3/8/8/3q4/4P3/8/8/Rn2K3 w - - 0 1
go perft 1
EOF
)
assert_not_contains "${capcolor_merge_child_output}" "a1b1:"
assert_not_contains "${capcolor_merge_child_output}" "e4d5:"
assert_contains "${capcolor_merge_child_output}" "e4e5:"

# Legacy shogiPawnDropMateIllegal and dropNoCheckmate = p agree exactly:
# the mating pawn drop is banned in both.
for variant in nc-alias-legacy nc-alias-newkey; do
  nc_alias_output=$(run_uci "$ENGINE" "$dropnomate_alias_ini" "$variant" <<EOF
position startpos
go perft 1
EOF
)
  assert_not_contains "${nc_alias_output}" "P@c4:"
done

# Opt-outs clear the ban whether the parent used the legacy key or the new
# key: the mating pawn drop returns in both children.
for variant in nc-alias-optout nc-alias-newoptout; do
  nc_optout_output=$(run_uci "$ENGINE" "$dropnomate_alias_ini" "$variant" <<EOF
position startpos
go perft 1
EOF
)
  assert_contains "${nc_optout_output}" "P@c4:"
done

bad_rider_range_val_output1=$(run_uci "$ENGINE" "$bad_rider_range_val_ini" "rider-range-val-1" </dev/null 2>&1 || true)
assert_contains "${bad_rider_range_val_output1}" "unknown variant 'rider-range-val-1'; keeping 'chess'"

bad_rider_range_val_output2=$(run_uci "$ENGINE" "$bad_rider_range_val_ini" "rider-range-val-2" </dev/null 2>&1 || true)
assert_contains "${bad_rider_range_val_output2}" "unknown variant 'rider-range-val-2'; keeping 'chess'"

bad_rider_range_val_output3=$(run_uci "$ENGINE" "$bad_rider_range_val_ini" "rider-range-val-3" </dev/null 2>&1 || true)
assert_contains "${bad_rider_range_val_output3}" "unknown variant 'rider-range-val-3'; keeping 'chess'"

bad_rider_range_val_output4=$(run_uci "$ENGINE" "$bad_rider_range_val_ini" "rider-range-val-4" </dev/null 2>&1 || true)
assert_contains "${bad_rider_range_val_output4}" "unknown variant 'rider-range-val-4'; keeping 'chess'"

bad_tuple_atom_output1=$(run_uci "$ENGINE" "$bad_tuple_atom_ini" "tuple-atom-1" </dev/null 2>&1 || true)
assert_contains "${bad_tuple_atom_output1}" "unknown variant 'tuple-atom-1'; keeping 'chess'"

bad_tuple_atom_output2=$(run_uci "$ENGINE" "$bad_tuple_atom_ini" "tuple-atom-2" </dev/null 2>&1 || true)
assert_contains "${bad_tuple_atom_output2}" "unknown variant 'tuple-atom-2'; keeping 'chess'"

bad_tuple_atom_output3=$(run_uci "$ENGINE" "$bad_tuple_atom_ini" "tuple-atom-3" </dev/null 2>&1 || true)
assert_contains "${bad_tuple_atom_output3}" "unknown variant 'tuple-atom-3'; keeping 'chess'"

bad_tuple_atom_output4=$(run_uci "$ENGINE" "$bad_tuple_atom_ini" "tuple-atom-4" </dev/null 2>&1 || true)
assert_contains "${bad_tuple_atom_output4}" "unknown variant 'tuple-atom-4'; keeping 'chess'"

bad_hook_output1=$(run_uci "$ENGINE" "$bad_hook_ini" "bad-hook-leg" </dev/null 2>&1 || true)
assert_contains "${bad_hook_output1}" "unknown variant 'bad-hook-leg'; keeping 'chess'"

bad_hook_output2=$(run_uci "$ENGINE" "$bad_hook_ini" "bad-hook-geometry" </dev/null 2>&1 || true)
assert_contains "${bad_hook_output2}" "unknown variant 'bad-hook-geometry'; keeping 'chess'"

bad_hook_output3=$(run_uci "$ENGINE" "$bad_hook_ini" "bad-hook-limit" </dev/null 2>&1 || true)
assert_contains "${bad_hook_output3}" "unknown variant 'bad-hook-limit'; keeping 'chess'"

bad_hook_no_turn_output=$(run_uci "$ENGINE" "$bad_hook_ini" "bad-hook-no-turn" </dev/null 2>&1 || true)
assert_contains "${bad_hook_no_turn_output}" "unknown variant 'bad-hook-no-turn'; keeping 'chess'"

bad_hook_output4=$(run_uci "$ENGINE" "$bad_hook_ini" "bad-hook-wrapped" </dev/null 2>&1 || true)
assert_contains "${bad_hook_output4}" "unknown variant 'bad-hook-wrapped'; keeping 'chess'"

bad_hook_toroidal_output=$(run_uci "$ENGINE" "$bad_hook_ini" "bad-hook-toroidal" </dev/null 2>&1 || true)
assert_contains "${bad_hook_toroidal_output}" "unknown variant 'bad-hook-toroidal'; keeping 'chess'"

bad_hook_output5=$(run_uci "$ENGINE" "$bad_hook_ini" "bad-hook-twoboards" </dev/null 2>&1 || true)
assert_contains "${bad_hook_output5}" "unknown variant 'bad-hook-twoboards'; keeping 'chess'"

bad_twostep_twoboards_output=$(run_uci "$ENGINE" "$bad_hook_ini" "bad-twostep-twoboards" </dev/null 2>&1 || true)
assert_contains "${bad_twostep_twoboards_output}" "unknown variant 'bad-twostep-twoboards'; keeping 'chess'"

bad_inherited_hook_output=$(run_uci "$ENGINE" "$bad_hook_ini" "bad-inherited-hook-wrapped" </dev/null 2>&1 || true)
assert_contains "${bad_inherited_hook_output}" "unknown variant 'bad-inherited-hook-wrapped'; keeping 'chess'"

bad_inherited_lion_output=$(run_uci "$ENGINE" "$bad_hook_ini" "bad-inherited-lion-twoboards" </dev/null 2>&1 || true)
assert_contains "${bad_inherited_lion_output}" "unknown variant 'bad-inherited-lion-twoboards'; keeping 'chess'"

inherited_hook_cleared_output=$(run_uci "$ENGINE" "$bad_hook_ini" "inherited-hook-cleared" </dev/null 2>&1 || true)
assert_not_contains "${inherited_hook_cleared_output}" "unknown variant 'inherited-hook-cleared'"

inherited_lion_cleared_output=$(run_uci "$ENGINE" "$bad_hook_ini" "inherited-lion-cleared" </dev/null 2>&1 || true)
assert_not_contains "${inherited_lion_cleared_output}" "unknown variant 'inherited-lion-cleared'"

bad_hook_check_output=$("${ENGINE}" check "${bad_hook_ini}" 2>&1 || true)
assert_contains "${bad_hook_check_output}" "hookMoves - Malformed hook legs"
assert_contains "${bad_hook_check_output}" "hookMoves - Invalid hook leg"
assert_contains "${bad_hook_check_output}" "hookMoves - Invalid hook capture limit"
assert_contains "${bad_hook_check_output}" "hookMoves - Hook legs have no valid direction pairs"
assert_contains "${bad_hook_check_output}" "twoStepMoves - Empty direction pair in list"
assert_contains "${bad_hook_check_output}" "hookMoves is not supported on wrapped boards."
assert_contains "${bad_hook_check_output}" "twoBoards is not supported with multi-capture twoStepMoves or :2 hookMoves."
assert_contains "${bad_hook_check_output}" "mandatoryPiecePromotion with pieceDemotion is not supported with twoStepMoves or hookMoves."

run_uci "$ENGINE" "$bad_hook_ini" "hook-twoboards-single" </dev/null >/dev/null
run_uci "$ENGINE" "$bad_hook_ini" "hook-bounded-diagonal-legs" </dev/null >/dev/null

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

load_inline_variants <<'INI'
[custom-removal-parent:fairy]
checking = false
castling = false
customPiece1 = a:W
pieceToCharTable = A:a
startFen = 8/8/8/8/8/8/8/A7 w - - 0 1

[custom-removal-child:custom-removal-parent]
customPiece1 = -
pieceToCharTable = -
startFen = 8/8/8/8/8/8/8/8 w - - 0 1

[royal-replace-parent:chess]
king = k:KN
startFen = rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1

[royal-replace-child:royal-replace-parent]
king = k

[royal-removal-child:royal-replace-parent]
king = -
checking = false
castling = false
pieceToCharTable = -
startFen = 8/8/8/8/8/8/8/8 w - - 0 1

[ep-redefinition-parent:chess]
promotionPawnTypes = n
enPassantTypes = n
doubleStepRegionWhite = N(*2)

[ep-redefinition-child:ep-redefinition-parent]
knight = n

[royal-mapping-parent:chess]
king = k:KN
castling = false
castlingKingPieceWhite = q
startFen = 4k3/8/8/8/8/8/8/4K3 w - - 0 1

[royal-mapping-child:royal-mapping-parent]
king = k
castling = true
startFen = 4k3/8/8/8/8/8/8/4Q2R w K - 0 1

[priority-clear-parent:chess]
pieceDrops = true
priorityDropTypes = p
startFen = 8/8/8/8/8/8/8/4K2k[PN] w - - 0 1

[priority-clear-child:priority-clear-parent]
priorityDropTypes = -
INI
priority_drop_clear_output=$(run_uci "$ENGINE" "$FSX_TMP_INI" priority-clear-child <<'UCI'
position startpos
go perft 1
UCI
)
assert_contains_literal "${priority_drop_clear_output}" "N@d4: 1" "cleared priority drop allows a knight"

custom_removal_output=$(run_uci "$ENGINE" "$FSX_TMP_INI" custom-removal-child </dev/null)
assert_contains "${custom_removal_output}" "^info string variant custom-removal-child "
assert_not_contains "${custom_removal_output}" "invalid configuration"

royal_replace_output=$(run_uci "$ENGINE" "$FSX_TMP_INI" royal-replace-child </dev/null)
assert_contains "${royal_replace_output}" "^info string variant royal-replace-child "
assert_not_contains "${royal_replace_output}" "invalid configuration"

royal_removal_output=$(run_uci "$ENGINE" "$FSX_TMP_INI" royal-removal-child </dev/null)
assert_contains "${royal_removal_output}" "^info string variant royal-removal-child "
assert_not_contains "${royal_removal_output}" "invalid configuration"

ep_redefinition_output=$(run_uci "$ENGINE" "$FSX_TMP_INI" ep-redefinition-child <<'UCI'
position fen 4k3/8/8/8/8/8/3N4/6K1 w - - 0 1 moves d2d4
d
UCI
)
assert_contains_literal "${ep_redefinition_output}" " b - d3 " "same-symbol redefinition preserves en-passant membership"

royal_mapping_output=$(run_uci "$ENGINE" "$FSX_TMP_INI" royal-mapping-child <<'UCI'
position startpos
go perft 1
UCI
)
assert_contains "${royal_mapping_output}" "^info string variant royal-mapping-child "
assert_not_contains "${royal_mapping_output}" "invalid configuration"
assert_contains "${royal_mapping_output}" "^e1g1: 1$" "preserved inherited castling piece mapping"

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

promotion_result_output=$(run_uci "$ENGINE" "$tmp_ini" "promotion-by-result-piece" <<EOF
position startpos
go perft 1
EOF
)
assert_contains "${promotion_result_output}" "a7a8q:"
assert_not_contains "${promotion_result_output}" "a7a8r:"
assert_contains "${promotion_result_output}" "a7a8n:"
assert_contains "${promotion_result_output}" "b7b8r:"
assert_not_contains "${promotion_result_output}" "b7b8q:"
assert_contains "${promotion_result_output}" "b7b8n:"

promotion_typed_only_output=$(run_uci "$ENGINE" "$tmp_ini" "promotion-by-result-piece-typed-only" <<EOF
position startpos
go perft 1
EOF
)
assert_contains "${promotion_typed_only_output}" "a7a8q:"
assert_not_contains "${promotion_typed_only_output}" "a7a8r:"

promotion_bare_output=$(run_uci "$ENGINE" "$tmp_ini" "promotion-region-bare-bitboard" <<EOF
position startpos
go perft 1
EOF
)
assert_contains "${promotion_bare_output}" "b7b8q:"
assert_contains "${promotion_bare_output}" "b7b8r:"

terminal_output=$(run_uci "$ENGINE" "" "" <<EOF
position fen 7k/5Q2/7K/8/8/8/8/8 b - - 0 1
go depth 1
EOF
)
assert_contains_literal "${terminal_output}" "bestmove (none)"

bench_output=$("${ENGINE}" bench 16 1 1 default nonsense 2>&1 || true)
assert_contains "${bench_output}" "Nodes searched  : "

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

# Verify rejection of lame universal leaper nU
cat > "$FSX_TMP_INI" <<'INI'
[test-lame-u:chess]
customPiece1 = u:nU
INI
lame_u_output=$(run_uci "$ENGINE" "$FSX_TMP_INI" test-lame-u </dev/null 2>&1 || true)
assert_contains "${lame_u_output}" "Unsupported Betza tuple modifier combination"

# Verify priorityDropTypes overrides parent variant instead of accumulating
cat > "$FSX_TMP_INI" <<'INI'
[priority-override-parent:chess]
pieceDrops = true
priorityDropTypes = p
startFen = 8/8/8/8/8/8/8/4K2k[PN] w - - 0 1

[priority-override-child:priority-override-parent]
priorityDropTypes = n
INI
priority_override_output=$(run_uci "$ENGINE" "$FSX_TMP_INI" priority-override-child <<'UCI'
position startpos
go perft 1
UCI
)
assert_contains_literal "${priority_override_output}" "N@d4: 1" "overridden priority drop allows N drop"
assert_not_contains "${priority_override_output}" "P@d4: 1"

# Verify non-negative bounds validation for counts and goals
for opt in pocketSize pointsGoal collinearN flagPieceCount antiRoyalCount pseudoRoyalCount nMoveHardLimitRule castlingForbiddenPlies swapForbiddenPlies nonRoyalDrawThreshold; do
    cat > "$FSX_TMP_INI" <<INI
[test-neg-${opt}:chess]
${opt} = -1
INI
    neg_count_output=$("${ENGINE}" check "$FSX_TMP_INI" 2>&1 || true)
    assert_contains "${neg_count_output}" "${opt} must be non-negative"
done

echo "parser regression tests passed"
