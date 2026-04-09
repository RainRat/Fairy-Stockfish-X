#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ENGINE="${1:-${SCRIPT_DIR}/../src/stockfish}"

TMP_VARIANTS="$(mktemp /tmp/self-destruct-special.XXXXXX.ini)"
trap 'rm -f "$TMP_VARIANTS"' EXIT

cat >"$TMP_VARIANTS" <<'EOF'
[self-destruct-special:chess]
king = -
customPiece1 = m:W
selfDestructTypes = m
checking = false
startFen = 8/8/8/8/8/8/8/M7 w - - 0 1
EOF

OUT="$(
cat <<EOF | "$ENGINE"
setoption name VariantPath value $TMP_VARIANTS
setoption name UCI_Variant value self-destruct-special
position startpos moves a1a1x
d
quit
EOF
)"

grep -F "Fen: 8/8/8/8/8/8/8/8 b - - 0 1" <<<"$OUT" >/dev/null || {
    echo "expected self-destruct move to remove the piece"
    echo "$OUT"
    exit 1
}

echo "self-destruct special regression passed"
