#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ENGINE="${1:-${SCRIPT_DIR}/../src/stockfish}"

tmpfile="$(mktemp "${TMPDIR:-/tmp}/fsx-nnue-export-XXXXXX.nnue")"
rm -f "$tmpfile"
trap 'rm -f "$tmpfile"' EXIT

output="$(python - "$ENGINE" "$tmpfile" <<'PY'
import subprocess
import sys

engine = sys.argv[1]
outfile = sys.argv[2]
script = (
    "uci\n"
    "setoption name Use NNUE value true\n"
    "setoption name EvalFile value chess-bogus.nnue\n"
    "isready\n"
    f"export_net {outfile}\n"
    "quit\n"
)
res = subprocess.run([engine], input=script, text=True, capture_output=True, check=True, timeout=20)
sys.stdout.write(res.stdout)
sys.stderr.write(res.stderr)
PY
)"

grep -Fq "Failed to export a net" <<<"$output"

if [[ -e "$tmpfile" ]]; then
    echo "unexpected export file created: $tmpfile" >&2
    exit 1
fi

echo "nnue export failure regression passed"
