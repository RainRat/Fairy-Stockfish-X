#!/usr/bin/env bash
set -euo pipefail

ENGINE="${1:-src/stockfish}"

output="$(python - "$ENGINE" 2>&1 <<'PY'
import subprocess
import sys

engine = sys.argv[1]
script = (
    "uci\n"
    "bench 16 1 1 current nodes\n"
    "quit\n"
)
res = subprocess.run([engine], input=script, text=True, capture_output=True)
sys.stdout.write(res.stdout)
sys.stdout.write(res.stderr)
PY
)"

printf '%s\n' "$output" | grep -F "Nodes searched" >/dev/null

echo "bench stdin regression passed"
