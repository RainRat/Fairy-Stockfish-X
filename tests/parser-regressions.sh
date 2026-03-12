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

echo "parser regression tests passed"
