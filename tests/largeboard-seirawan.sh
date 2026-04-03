#!/bin/bash

set -euo pipefail

error() {
  echo "largeboard seirawan regression failed on line $1"
  [[ -n "${TMP_VARIANT_PATH:-}" ]] && rm -f "${TMP_VARIANT_PATH}"
  exit 1
}
trap 'error ${LINENO}' ERR

PYTHON=${PYTHON:-python3}

TMP_VARIANT_PATH=$(mktemp /tmp/fsx-seirawan10-XXXXXX.ini)
cat >"${TMP_VARIANT_PATH}" <<'INI'
[seirawan10:chess]
gating = true
seirawanGating = true
maxRank = 10
maxFile = 10
customPiece1 = h:N
pieceToCharTable = H:h
startFen = rnbqkbnr2/pppppppppp/10/10/10/10/10/10/PPPPPPPPPP/RNBQKBNR1R[Hh] w KQ|1000100001/0000000000 - 0 1
INI

"${PYTHON}" - <<'PY' "${TMP_VARIANT_PATH}"
import sys
import pyffish as sf

variant_path = sys.argv[1]
with open(variant_path, 'r', encoding='utf-8') as f:
    sf.load_variant_config(f.read())

fen = sf.start_fen("seirawan10")
assert sf.validate_fen(fen, "seirawan10", False) == sf.FEN_OK, fen
moves = sf.legal_moves("seirawan10", fen, [])
assert "j1i1h" in moves, moves
assert "a2a3h" in moves, moves
assert sf.get_fen("seirawan10", fen, []) == fen
after = sf.get_fen("seirawan10", fen, ["j1i1h"])
assert "[h]" in after, after
assert "|0000000000/0000000000" in after, after
PY

rm -f "${TMP_VARIANT_PATH}"
unset TMP_VARIANT_PATH

echo "largeboard seirawan regression passed"
