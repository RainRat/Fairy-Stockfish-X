#!/bin/bash

source "$(dirname "$0")/common.sh"

echo "standard piece value phase regression test started"

tmp_ini=$(create_tmp_ini <<'INI'
[knight-low-eg:chess]
pieceValueMg = n:1000
pieceValueEg = n:1
INI
)

extract_material_eg() {
  awk '/^\|   Material / { print $(NF-1) }' | tail -n1
}

output=$(run_uci "position fen 4k3/8/8/3N4/8/8/8/4K3 w - - 0 1\neval" "${tmp_ini}")
material_eg=$(printf '%s\n' "${output}" | extract_material_eg)

if [[ -z "${material_eg}" ]]; then
  echo "Failed to extract material eval"
  echo "${output}"
  exit 1
fi

python3 - "${material_eg}" <<'PY'
import sys
score = float(sys.argv[1])
if score <= 0.10:
    raise SystemExit(f"expected positive endgame material contribution, got {score}")
PY

echo "standard piece value phase regression passed"
