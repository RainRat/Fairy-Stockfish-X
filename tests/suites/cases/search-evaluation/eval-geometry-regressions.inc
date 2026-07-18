#!/bin/bash

set -euo pipefail

error() {
  echo "eval geometry regression test failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./stockfish}

extract_eval() {
  sed -n 's/^Final evaluation[[:space:]]*//p' | tail -n1 | awk '{print $1}'
}

run_eval() {
  local variant_path="$1"
  local variant="$2"
  local fen="$3"
  cat <<CMDS | "${ENGINE}" | extract_eval
uci
setoption name VariantPath value ${variant_path}
setoption name UCI_Variant value ${variant}
position fen ${fen}
eval
quit
CMDS
}

run_trace() {
  local variant_path="$1"
  local variant="$2"
  local fen="$3"
  cat <<CMDS | "${ENGINE}"
uci
setoption name Use NNUE value true
setoption name EvalFile value src/nn-3475407dc199.nnue
setoption name VariantPath value ${variant_path}
setoption name UCI_Variant value ${variant}
position fen ${fen}
eval
quit
CMDS
}

tmp_ini=$(mktemp)
trap 'rm -f "${tmp_ini}"' EXIT

cat > "${tmp_ini}" <<'INI'
[narrow-shelter:chess]
maxFile = 3
maxRank = 8
castling = false
startFen = 3/3/3/3/3/3/PPP/K1k w - - 0 1

[fairy-eval:chess]
maxFile = 8
maxRank = 8
castling = false
customPiece1 = m:N
promotionPieceTypes = -
startFen = 4k3/8/8/8/8/8/4M3/4K3 w - - 0 1

[mini-bishop:chess]
maxFile = 5
maxRank = 5
castling = false
promotionPieceTypes = -
startFen = 5/5/5/5/5 w - - 0 1
INI

# Narrow boards should evaluate without tripping invalid shelter clamping.
narrow_eval=$(run_eval "${tmp_ini}" "narrow-shelter" "3/3/3/3/3/3/PPP/K1k w - - 0 1")
[[ -n "${narrow_eval}" ]]

# Fairy-piece threat/mobility paths should evaluate successfully too.
fairy_eval=$(run_eval "${tmp_ini}" "fairy-eval" "4k3/8/8/8/4r3/8/4M3/4K3 w - - 0 1")
[[ -n "${fairy_eval}" ]]

# Non-8x8 long-diagonal bishop bonus should use the runtime board center, not literal 8x8 center squares.
bishop_mg=$(cat <<CMDS | "${ENGINE}" | awk '/^\|    Bishops \|/ { print $4 }' | tail -n1
uci
setoption name VariantPath value ${tmp_ini}
setoption name UCI_Variant value mini-bishop
position fen 5/5/2B2/5/4k w - - 0 1
eval
quit
CMDS
)
[[ "${bishop_mg}" == "0.00" ]]

# NNUE trace headers should align to the same four-column width as the body.
trace_output=$(run_trace "${tmp_ini}" "chess" "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
[[ "${trace_output}" == *"|   Bucket   |  Material  | Positional |   Total    |"* ]]
[[ "${trace_output}" == *"|            |   (PSQT)   |  (Layers)  |            |"* ]]

echo "eval geometry regression tests passed"
