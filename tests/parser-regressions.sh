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

# Fail if output contains unexpected internal errors
if printf '%s\n' "${check_output}" | grep -Eq "PieceTypeBitboardGroup declaration|Invalid value.*whitePieceDropRegion|Error parsing|unterminated"; then
  echo "Unexpected error in parser output:"
  printf '%s\n' "${check_output}"
  exit 1
fi

# Fail if output contains empty field errors (these shouldn't happen for empty fields anymore)
if printf '%s\n' "${check_output}" | grep -Eq "piecePoints - Invalid piece type: $|promotionLimit - Invalid piece type: $|priorityDropTypes - Invalid piece type: $|virtualDropLimit - Invalid piece type: $"; then
  echo "Detected empty field error which should be ignored:"
  printf '%s\n' "${check_output}"
  exit 1
fi

# Verify new incompatibility warnings
verify_warning() {
  local pattern="$1"
  local label="$2"
  if ! printf '%s\n' "${check_output}" | grep -qF "${pattern}"; then
    echo "Failed: ${label}"
    echo "Expected warning not found: ${pattern}"
    echo "Full output was:"
    printf '%s\n' "${check_output}"
    exit 1
  fi
}

verify_warning "wallingRule and seirawanGating are incompatible." "seirawanGating check"
verify_warning "wallingRule and potions are incompatible." "potions check"
verify_warning "wallingRule=duck and petrifyOnCaptureTypes are incompatible." "petrify check"
verify_warning "pieceDrops and any walling are incompatible." "freeDrops check"

tuple_output=$(cat <<CMDS | "${ENGINE}" 2>&1
uci
setoption name VariantPath value ${tmp_ini}
setoption name UCI_Variant value tuple-nonsquare
position startpos
go perft 1
quit
CMDS
)

if printf '%s\n' "${tuple_output}" | grep -q "No piece char found for custom piece"; then
  echo "Tuple test failed:"
  printf '%s\n' "${tuple_output}"
  exit 1
fi

terminal_output=$(cat <<'CMDS' | "${ENGINE}" 2>&1
uci
position fen 7k/5Q2/7K/8/8/8/8/8 b - - 0 1
go depth 1
quit
CMDS
)

if ! printf '%s\n' "${terminal_output}" | grep -q "bestmove (none)"; then
  echo "Terminal position test failed:"
  printf '%s\n' "${terminal_output}"
  exit 1
fi

bench_output=$("${ENGINE}" bench 16 1 1 default nonsense 2>&1 || true)
if ! printf '%s\n' "${bench_output}" | grep -q "Nodes searched  : "; then
  echo "Bench test failed:"
  printf '%s\n' "${bench_output}"
  exit 1
fi

echo "parser regression tests passed"
