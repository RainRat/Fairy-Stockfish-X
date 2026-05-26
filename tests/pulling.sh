#!/usr/bin/env bash

set -euo pipefail

error() {
  echo "pulling regression failed on line $1" >&2
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ENGINE="${1:-${SCRIPT_DIR}/../src/stockfish}"

source "${SCRIPT_DIR}/lib/uci.sh"

TMP_INI=$(mktemp)
trap 'rm -f "${TMP_INI}"' EXIT

cat > "${TMP_INI}" <<'INI'
[pull-basic:fairy]
maxFile = e
maxRank = 5
castling = false
checking = false
king = -
pieceToCharTable = -
customPiece1 = a:mW
customPiece2 = b:mW
customPiece3 = c:mW
pullingStrength = a:3 b:1 c:3
startFen = 5/5/5/5/5 w - - 0 1

[pull-allow-checks:fairy]
maxFile = e
maxRank = 5
castling = false
checking = false
allowChecks = true
pieceToCharTable = K...A...R...k...b...r...
king = k
customPiece1 = a:mW
customPiece2 = b:mW
customPiece3 = r:R
pullingStrength = a:3 b:1
startFen = 5/5/5/5/5 w - - 0 1
INI

out=$(run_uci "${ENGINE}" "${TMP_INI}" pull-basic <<'UCI'
position fen 5/5/2b2/2A2/5 w - - 0 1
go perft 1
UCI
)
assert_contains "$out" "^c2d2: 1$"
assert_contains "$out" "^c2d2,c3: 1$"

out=$(run_uci "${ENGINE}" "${TMP_INI}" pull-basic <<'UCI'
position fen 5/5/2c2/2A2/5 w - - 0 1
go perft 1
UCI
)
assert_not_contains "$out" "^c2d2,c3: 1$"

out=$(run_uci "${ENGINE}" "${TMP_INI}" pull-basic <<'UCI'
position fen 5/5/2b2/2A2/5 w - - 0 1 moves c2d2,c3
d
UCI
)
assert_fen "$out" "5/5/5/2bA1/5 b - - 1 1"

echo "pulling ok"
