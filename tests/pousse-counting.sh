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

cfg = open(os.path.join(os.environ["ROOT"], "src", "variants.ini"), encoding="utf-8").read()
sf.load_variant_config(cfg)

# A completed straight should not end Pousse early while moves remain.
fen = "AAAAAA/5/5/5/5/5[aaaaaaaaaaaaaaaaaa] b - - 0 1"
if sf.is_immediate_game_end("pousse", fen, [])[0]:
    raise SystemExit(f"unexpected immediate Pousse end for {fen}")
if sf.is_optional_game_end("pousse", fen, [])[0]:
    raise SystemExit(f"unexpected optional Pousse end for {fen}")
if not sf.legal_moves("pousse", fen, []):
    raise SystemExit(f"expected legal Pousse moves for {fen}")

# A no-move position should be a loss, not a pass.
stalemate = "AaAaAa/aAaAaA/AaAaAa/aAaAaA/AaAaAa/aAaAaA[] w - - 0 1"
if sf.legal_moves("pousse", stalemate, []):
    raise SystemExit(f"expected no legal Pousse moves for stalemate board: {stalemate}")
if sf.game_result("pousse", stalemate, []) >= 0:
    raise SystemExit(f"expected stalemate loss for Pousse board: {stalemate}")
PY

echo "pousse counting tests passed"
