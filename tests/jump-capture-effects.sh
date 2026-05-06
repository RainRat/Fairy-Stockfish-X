#!/usr/bin/env bash
set -euo pipefail

ENGINE="${1:-src/stockfish}"

tmp_ini="$(mktemp)"
trap 'rm -f "$tmp_ini"' EXIT

cat >"$tmp_ini" <<'EOF'
[checkers]
customPiece1 = m:mfFfc{hurdles: 1,1; pre: 1,1; post: 1,1; capture: locust_first; hurdle_types:enemy}F
customPiece2 = k:mFc{hurdles: 1,1; pre: 1,1; post: 1,1; capture: locust_first; hurdle_types:enemy}F
startFen = 1m1m1m1m/m1m1m1m1/1m1m1m1m/8/8/M1M1M1M1/1M1M1M1M/M1M1M1M1 w - - 0 1
promotionPawnTypes = m
promotionPieceTypes = k
mustCapture = true
checking = false
forcedJumpContinuation = true
stalemateValue = loss
nMoveRule = 0
nFoldRule = 3

[jumpatomic:checkers]
blastOnCapture = true
blastCenter = true
blastDiagonals = true

[jumpduck:checkers]
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

grep -Fq "c3e5: 1" <<<"$duck_out"
grep -Fq "c3e5,e5d4: 1" <<<"$duck_out"
