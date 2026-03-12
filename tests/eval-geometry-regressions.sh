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
INI

# Narrow boards should evaluate without tripping invalid shelter clamping.
narrow_eval=$(run_eval "${tmp_ini}" "narrow-shelter" "3/3/3/3/3/3/PPP/K1k w - - 0 1")
[[ -n "${narrow_eval}" ]]

# Fairy-piece threat/mobility paths should evaluate successfully too.
fairy_eval=$(run_eval "${tmp_ini}" "fairy-eval" "4k3/8/8/8/4r3/8/4M3/4K3 w - - 0 1")
[[ -n "${fairy_eval}" ]]

echo "eval geometry regression tests passed"
