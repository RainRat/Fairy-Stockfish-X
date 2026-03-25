#!/usr/bin/env bash
set -euo pipefail

ENGINE="${1:-./src/stockfish}"
VARIANTS="${2:-./src/variants.ini}"

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT

run_cmds() {
  printf 'uci\nsetoption name VariantPath value %s\n%s\nquit\n' "$VARIANTS" "$1" | "$ENGINE"
}

echo "toot-otto test started"

out=$(run_cmds "setoption name UCI_Variant value toot-otto
position startpos
go perft 1")

grep -F "info string variant toot-otto" <<<"$out" >/dev/null || {
  echo "toot-otto variant failed to load" >&2
  exit 1
}

grep -F "T@a1: 1" <<<"$out" >/dev/null || {
  echo "expected T drop from start position" >&2
  exit 1
}

grep -F "O@a1: 1" <<<"$out" >/dev/null || {
  echo "expected O drop from start position" >&2
  exit 1
}

echo "toot-otto test OK"
