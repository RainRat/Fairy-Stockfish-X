#!/bin/bash

set -euo pipefail

error() {
  echo "pousse counting test failed on line $1" >&2
  exit 1
}
trap 'error ${LINENO}' ERR

ROOT=$(cd "$(dirname "$0")/.." && pwd)

ROOT="$ROOT" python3 - <<'PY'
import os
import pyffish as sf

cfg = open(os.path.join(os.environ["ROOT"], "src", "variants-incomplete.ini"), encoding="utf-8").read()
sf.load_variant_config(cfg)

# A completed straight should not end Pousse early while moves remain.
fen = "AAAAAA/5/5/5/5/5[aaaaaaaaaaaaaaaaaa] b - - 0 1"
if sf.is_immediate_game_end("pousse", fen, [])[0]:
    raise SystemExit(f"unexpected immediate Pousse end for {fen}")
if sf.is_optional_game_end("pousse", fen, [])[0]:
    raise SystemExit(f"unexpected optional Pousse end for {fen}")
if not sf.legal_moves("pousse", fen, []):
    raise SystemExit(f"expected legal Pousse moves for {fen}")

# Full-board adjudication compares completed straights instead of first-connect.
white_surplus = "AAAAAA/AAAAAA/AAAAAA/AAAAAA/aaaaaa/aaaaaa[] b - - 0 1"
black_surplus = "AAAAAA/AAAAAA/aaaaaa/aaaaaa/aaaaaa/aaaaaa[] b - - 0 1"
tie = "AAAAAA/AAAAAA/AAAAAA/aaaaaa/aaaaaa/aaaaaa[] b - - 0 1"

if sf.game_result("pousse", white_surplus, []) >= 0:
    raise SystemExit(f"expected side-to-move loss for white-surplus board: {white_surplus}")
if sf.game_result("pousse", black_surplus, []) <= 0:
    raise SystemExit(f"expected side-to-move win for black-surplus board: {black_surplus}")
if sf.game_result("pousse", tie, []) != sf.VALUE_DRAW:
    raise SystemExit(f"expected draw for tied-straights board: {tie}")
PY

echo "pousse counting tests passed"
