#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ENGINE="${1:-${SCRIPT_DIR}/../../../../src/stockfish}"

tmpfile="$(mktemp "${TMPDIR:-/tmp}/fsx-nnue-export-XXXXXX.nnue")"
rm -f "$tmpfile"
trap 'rm -f "$tmpfile"' EXIT

output="$( {
    printf 'uci\n'
    printf 'setoption name Use NNUE value true\n'
    printf 'setoption name EvalFile value chess-bogus.nnue\n'
    printf 'isready\n'
    printf 'export_net %s\n' "$tmpfile"
    printf 'quit\n'
  } | timeout 20 "$ENGINE" 2>&1 )"

grep -Fq "Failed to export a net" <<<"$output"

if [[ -e "$tmpfile" ]]; then
    echo "unexpected export file created: $tmpfile" >&2
    exit 1
fi

echo "nnue export failure regression passed"
