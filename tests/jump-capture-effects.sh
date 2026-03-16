#!/usr/bin/env bash
set -euo pipefail

ENGINE="${1:-src/stockfish}"

tmp_ini="$(mktemp)"
trap 'rm -f "$tmp_ini"' EXIT

cat >"$tmp_ini" <<'EOF'
[checkersmini]
customPiece1 = m:fFfA
customPiece2 = k:FA
promotionPawnTypes = m
promotionPieceTypes = k
mustCapture = true
checking = false
jumpCaptureTypes = *
forcedJumpContinuation = true
stalemateValue = loss
nMoveRule = 0
nFoldRule = 3

[jumpatomic:checkersmini]
blastOnCapture = true
blastCenter = true
blastDiagonals = true

[jumpduck:checkersmini]
wallingRule = duck
wallingSide = wb
EOF

atomic_out="$("$ENGINE" <<EOF
setoption name VariantPath value $tmp_ini
setoption name UCI_Variant value jumpatomic
position fen 8/8/5m2/8/3m4/2M5/8/7K w - - 0 1 moves c3e5
d
quit
EOF
)"

grep -Fq "Fen: 8/8/5m2/8/8/8/8/7K b - - 0 1" <<<"$atomic_out"

duck_out="$("$ENGINE" <<EOF
setoption name VariantPath value $tmp_ini
setoption name UCI_Variant value jumpduck
position fen 8/8/5m2/8/3m4/2M5/8/7K w - - 0 1
go perft 1
quit
EOF
)"

grep -Fq "c3e5,d4: 1" <<<"$duck_out"
