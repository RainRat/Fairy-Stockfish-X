#!/bin/bash

set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)

ROOT="$ROOT" python3 - <<'PY'
import os
import pyffish as sf

cfg = open(os.path.join(os.environ["ROOT"], "src", "variants-incomplete.ini"), encoding='utf-8').read()
sf.load_variant_config(cfg)

cases = [
    ("7k/8/8/8/8/8/8/7K w - - 0 1", -sf.VALUE_MATE),
    ("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1", -sf.VALUE_MATE),
]

for fen, expected in cases:
    result = sf.game_result("hindustani", fen, [])
    if result != expected:
        raise SystemExit(f"unexpected Hindustani result for {fen}: got {result}, expected {expected}")
PY
