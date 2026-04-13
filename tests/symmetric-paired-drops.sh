#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ENGINE="${1:-${SCRIPT_DIR}/../src/stockfish}"

TMP_VARIANTS="$(mktemp)"
trap 'rm -f "$TMP_VARIANTS"' EXIT

cat >"$TMP_VARIANTS" <<'EOF'
[pairdrop:chess]
pieceDrops = true
symmetricDropTypes = r
EOF

PERFT_OUT="$(
printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value pairdrop\nposition fen 4k3/8/8/8/8/8/8/4K3[RR] w - - 0 1\ngo perft 1\nquit\n' "$TMP_VARIANTS" | "$ENGINE"
)"

grep -q 'R@a4,h4: 1' <<<"$PERFT_OUT"
grep -q 'R@d4,e4: 1' <<<"$PERFT_OUT"
if grep -q 'R@a4: 1' <<<"$PERFT_OUT"; then
    echo "unexpected single drop generated"
    exit 1
fi

BOARD_OUT="$(
printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value pairdrop\nposition fen 4k3/8/8/8/8/8/8/4K3[RR] w - - 0 1 moves R@a4,h4\nd\nquit\n' "$TMP_VARIANTS" | "$ENGINE"
)"

grep -q 'Fen: 4k3/8/8/8/R6R/8/8/4K3\[] b - - 0 1' <<<"$BOARD_OUT"
