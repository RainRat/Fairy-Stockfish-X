#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ENGINE="${1:-${SCRIPT_DIR}/../src/stockfish}"
VARIANT_PATH="${2:-${SCRIPT_DIR}/../src/variants.ini}"

output="$(
  printf 'setoption name VariantPath value %s\nsetoption name UCI_Variant value gale\nposition startpos\ngo perft 1\nquit\n' "$VARIANT_PATH" \
    | "$ENGINE" 2>&1
)"

if grep -q "Variant 'gale' exceeds build board limits" <<<"$output"; then
  echo "skip: gale requires VERY_LARGE_BOARDS"
  exit 0
fi

if grep -q "No such variant: gale" <<<"$output"; then
  echo "skip: gale unavailable in this binary"
  exit 0
fi

grep -q "info string variant gale " <<<"$output"
grep -q "Nodes searched: 41" <<<"$output"
