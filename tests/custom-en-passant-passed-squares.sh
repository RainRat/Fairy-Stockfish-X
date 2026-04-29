#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)

error() {
  echo "custom en passant passed squares regression failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

export FSX_REPO_ROOT="${REPO_ROOT}"

python3 - <<'PY'
import os
import sys
sys.path.insert(0, os.environ["FSX_REPO_ROOT"])
import pyffish as sf

cfg = """
[custom-ep-all:chess]
customPiece1 = a:mWifemR3
customPiece2 = s:fK
pawn = -
enPassantTypes = as
tripleStepRegionWhite = *2
tripleStepRegionBlack = *7
enPassantRegionWhite = *1 *2 *3 *4 *5 *6 *7 *8
enPassantRegionBlack = *1 *2 *3 *4 *5 *6 *7 *8
startFen = 8/8/8/2s1s3/8/8/3A4/8 w - - 0 1
checking = false
flagPiece = -

[custom-ep-first:custom-ep-all]
enPassantPassedSquares = first
"""

sf.load_variant_config(cfg)

def is_capture_safe(variant, fen, move):
    try:
        return sf.is_capture(variant, fen, [], move)
    except ValueError:
        return False

fen = sf.start_fen("custom-ep-all")
fen_all = sf.get_fen("custom-ep-all", fen, ["d2d5"])
assert " b - d3d4d5 " in fen_all, fen_all
assert is_capture_safe("custom-ep-all", fen_all, "c5d4"), fen_all
assert is_capture_safe("custom-ep-all", fen_all, "e5d4"), fen_all

fen_first = sf.get_fen("custom-ep-first", fen, ["d2d5"])
assert " b - d3 " in fen_first, fen_first
assert not is_capture_safe("custom-ep-first", fen_first, "c5d4"), fen_first
assert not is_capture_safe("custom-ep-first", fen_first, "e5d4"), fen_first

print("custom en passant passed squares regression tests passed")
PY
