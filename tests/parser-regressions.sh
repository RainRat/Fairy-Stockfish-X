#!/bin/bash

set -euo pipefail

error() {
  echo "parser regression test failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./stockfish}

tmp_ini=$(mktemp)
trap 'rm -f "${tmp_ini}"' EXIT

cat > "${tmp_ini}" <<'INI'
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
INI

echo "parser regression tests started"

check_output=$("${ENGINE}" check "${tmp_ini}" 2>&1 || true)
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
"""
)

pyffish.validate_fen("10/10/10/10/10/10/10/1R3K2R1 w JQ - 0 1", "castdiag-empty", False)
pyffish.validate_fen("10/10/10/10/10/10/10/1R3K3N w JQ - 0 1", "castdiag-wrongpiece", False)
PY
)

if ! echo "${castling_diag_output}" | grep -q "No castling rook on file J for flag J."; then
  echo "${castling_diag_output}"
  exit 1
fi

if ! echo "${castling_diag_output}" | grep -q "Flag J refers to file J, but that square does not contain a WHITE castling rook."; then
  echo "${castling_diag_output}"
  exit 1
fi

echo "parser regression tests passed"
