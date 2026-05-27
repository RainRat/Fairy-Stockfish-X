#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "${BASH_SOURCE[0]}")/lib/uci.sh"

ENGINE=$(default_engine "${1:-}")
VARIANTS_MAIN=$(default_variants "${2:-}")
VARIANTS_INCOMPLETE="${3:-$ROOT_DIR/src/variants-incomplete.ini}"
ENGINE_LARGE="${4:-$ROOT_DIR/src/stockfish-large}"
ENGINE_VLB="${5:-$ROOT_DIR/src/stockfish-vlb}"

echo "dots and boxes regression started"

out=$(run_uci "$ENGINE" "$VARIANTS_INCOMPLETE" dots-boxes-2x2 <<'EOF'
position startpos
go perft 1
EOF
)
assert_nodes "$out" 12

out=$(run_uci "$ENGINE" "$VARIANTS_MAIN" dots-boxes-7x7 <<'EOF'
position startpos
go perft 1
EOF
)
assert_nodes "$out" 24

if [[ -x "${ENGINE_LARGE}" ]]; then
  out=$(run_uci "$ENGINE_LARGE" "$VARIANTS_MAIN" dots-boxes-9x9 <<'EOF'
position startpos
go perft 1
EOF
)
  assert_contains_literal "$out" "info string variant dots-boxes-9x9 "
  assert_nodes "$out" 40
fi

if [[ -x "${ENGINE_VLB}" ]]; then
  out=$(run_uci "$ENGINE_VLB" "$VARIANTS_MAIN" dots-boxes-15x15 <<'EOF'
position startpos
go perft 1
EOF
)
  assert_contains_literal "$out" "info string variant dots-boxes-15x15 "
  assert_nodes "$out" 112
fi

ROOT_DIR="$ROOT_DIR" python3 - <<'PY'
import pyffish as sf
import os
from pathlib import Path

root = Path(os.environ["ROOT_DIR"])
sf.load_variant_config((root / "src" / "variants.ini").read_text())
sf.load_variant_config((root / "src" / "variants-incomplete.ini").read_text())

assert sf.game_result("dots-boxes-2x2", "*****/*B*B*/*****/*B*B*/***** w - - 12 7", []) > 0
assert sf.game_result("dots-boxes-2x2", "*****/*B*B*/*****/*B*b*/***** w - - 12 7", []) > 0
assert sf.game_result("dots-boxes-2x2", "*****/*B*B*/*****/*b*b*/***** w - - 12 7", []) == sf.VALUE_DRAW
assert sf.game_result("dots-boxes-2x2", "*****/*B*b*/*****/*b*b*/***** w - - 12 7", []) < 0
assert sf.legal_moves("dots-boxes-2x2", "*****/*B*B*/*****/*B*B*/***** w - - 12 7", []) == []
assert sorted(sf.legal_moves("dots-boxes-2x2", "***1*/*b*2/***1*/5/*1*1* b - - 4 3", [])) == sorted([
    "0000,b1", "0000,d1", "0000,a2", "0000,c2", "0000,e2", "0000,d3", "0000,e4", "0000,d5"
])
PY

echo "dots and boxes regression passed"
