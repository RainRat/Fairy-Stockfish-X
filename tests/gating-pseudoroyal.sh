#!/bin/bash
# Regression test: gated pseudo-royals must respect attacked-square legality.

set -euo pipefail

error() {
  echo "gating-pseudoroyal testing failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
ENGINE="${1:-${REPO_ROOT}/src/stockfish}"

echo "gating-pseudoroyal testing started"

VARIANT_FILE=$(mktemp)
OUT_FILE=$(mktemp)
trap 'rm -f "$VARIANT_FILE" "$OUT_FILE"' EXIT

cat > "$VARIANT_FILE" <<'VAR'
[gate_pseudoroyal_illegal:seirawan]
pseudoRoyalTypes = h
pseudoRoyalCount = 99
castling = false
startFen = 4k3/8/8/8/8/8/b7/1N2K3[H] w B - 0 1

[gate_pseudoroyal_capture:seirawan]
pseudoRoyalTypes = h
pseudoRoyalCount = 99
castling = false
startFen = 4k3/8/8/8/8/8/1b6/2B1K3[H] w C - 0 1
VAR

cat <<CMDS | "$ENGINE" > "$OUT_FILE" 2>&1
uci
setoption name VariantPath value $VARIANT_FILE
setoption name UCI_Variant value gate_pseudoroyal_illegal
position startpos
go perft 1
setoption name UCI_Variant value gate_pseudoroyal_capture
position startpos
go perft 1
quit
CMDS

grep -Fq "b1a3: 1" "$OUT_FILE"
if grep -Fq "b1a3h: 1" "$OUT_FILE"; then
  echo "illegal attacked gating move was generated"
  exit 1
fi

grep -Fq "c1b2h: 1" "$OUT_FILE"

echo "gating-pseudoroyal testing OK"