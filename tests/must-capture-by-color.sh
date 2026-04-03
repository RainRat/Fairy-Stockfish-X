#!/bin/bash
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
ENGINE="${1:-${REPO_ROOT}/src/stockfish}"
set -euo pipefail

# cd "$(dirname "$0")/../src" # removed for absolute paths

tmp_ini=$(mktemp)
trap 'rm -f "$tmp_ini"' EXIT

cat > "$tmp_ini" <<'INI'
[asymmustcapture:chess]
mustCaptureWhite = true
mustCaptureBlack = false
startFen = 4k3/8/8/3p4/4P3/8/8/4K3 w - - 0 1
INI

out_white=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value asymmustcapture\nposition startpos\ngo perft 1\nquit\n' "$tmp_ini" | "$ENGINE")
grep -q "e4d5:" <<<"$out_white"
! grep -q "e4e5:" <<<"$out_white"

grep -q "Nodes searched: 1" <<<"$out_white"

black_fen='4k3/8/8/4p3/3P4/8/8/4K3 b - - 0 1'
out_black=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value asymmustcapture\nposition fen %s\ngo perft 1\nquit\n' "$tmp_ini" "$black_fen" | "$ENGINE")
grep -q "e5d4:" <<<"$out_black"
grep -q "e5e4:" <<<"$out_black"

echo "mustCaptureByColor test OK"