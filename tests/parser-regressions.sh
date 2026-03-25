#!/bin/bash

source "$(dirname "$0")/common.sh"

echo "parser regression tests started"

tmp_ini=$(create_tmp_ini <<'INI'
[ptbg-no-semicolon:chess]
pieceDrops = true
whitePieceDropRegion = P(a8)

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
INI
)

check_output=$("${STOCKFISH}" check "${tmp_ini}" 2>&1 || true)
if echo "${check_output}" | grep -Eq "PieceTypeBitboardGroup declaration|Invalid value.*whitePieceDropRegion|Error parsing|unterminated"; then
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
verify_warning "Toroidal boards do not support x/z rider modifiers in customPiece1." "toroidal x/z rejection"

nonking_ini=$(create_tmp_ini <<'INI'
[nonking-inline-betza:chess]
rook = r:R3
INI
)

nonking_output=$("${STOCKFISH}" check "${nonking_ini}" 2>&1 || true)
if ! printf '%s\n' "${nonking_output}" | grep -qF "rook only supports a piece letter here. Use customPieceN = r:R3 and remap rook to that letter instead."; then
  echo "Failed: non-king inline Betza rejection"
  printf '%s\n' "${nonking_output}"
  exit 1
fi

tuple_output=$(run_uci "setoption name UCI_Variant value tuple-nonsquare\nposition startpos\ngo perft 1" "${tmp_ini}")
if echo "${tuple_output}" | grep -q "No piece char found for custom piece"; then
  echo "${tuple_output}"
  exit 1
fi

terminal_output=$(run_uci "position fen 7k/5Q2/7K/8/8/8/8/8 b - - 0 1\ngo depth 1")
if ! echo "${terminal_output}" | grep -q "bestmove (none)"; then
  echo "${terminal_output}"
  exit 1
fi

bench_output=$("${STOCKFISH}" bench 16 1 1 default nonsense 2>&1 || true)
if ! echo "${bench_output}" | grep -q "Nodes searched  : "; then
  echo "${bench_output}"
  exit 1
fi

echo "parser regression tests passed"
