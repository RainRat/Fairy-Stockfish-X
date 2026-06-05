#!/bin/bash

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
export ROOT_DIR
source "${ROOT_DIR}/tests/lib/uci.sh"
setup_test_context "${1:-}" "${2:-}" "rider regressions"

test_rose() {
  if ! variant_available "$ENGINE" rose "$VARIANTS"; then
    echo "rose variant not available in this build; skipping rose regression"
    return 0
  fi

  load_inline_variants <<'INI'
[rose-empty:chess]
king = -
checking = false
customPiece1 = a:rose
pieceToCharTable = A:a
startFen = 8/8/8/8/8/8/8/A7 w - - 0 1

[rose-block-b3:rose-empty]
startFen = 8/8/8/8/8/1p6/8/A7 w - - 0 1

[rose-block-c2:rose-empty]
startFen = 8/8/8/8/8/8/2p5/A7 w - - 0 1

[rose-block-both:rose-empty]
startFen = 8/8/8/8/8/1p6/2p5/A7 w - - 0 1
INI

  local out
  out=$(run_uci "$ENGINE" "$TMP_VARIANTS" rose-empty <<'EOF'
position startpos
go perft 1
EOF
)
  assert_contains "$out" "^a1b3: 1$"
  assert_contains "$out" "^a1c2: 1$"
  assert_contains "$out" "^a1d4: 1$"
  assert_contains "$out" "^a1e1: 1$"

  out=$(run_uci "$ENGINE" "$TMP_VARIANTS" rose-block-b3 <<'EOF'
position startpos
go perft 1
EOF
)
  assert_contains "$out" "^a1d4: 1$"

  out=$(run_uci "$ENGINE" "$TMP_VARIANTS" rose-block-c2 <<'EOF'
position startpos
go perft 1
EOF
)
  assert_contains "$out" "^a1d4: 1$"

  out=$(run_uci "$ENGINE" "$TMP_VARIANTS" rose-block-both <<'EOF'
position startpos
go perft 1
EOF
)
  assert_not_contains "$out" "^a1d4: 1$"
  assert_contains "$out" "^a1b3: 1$"
  assert_contains "$out" "^a1c2: 1$"
}

test_bent_riders() {
  if ! variant_available "$ENGINE" bent-riders "$VARIANTS"; then
    echo "bent-riders variant not available in this build; skipping bent-riders regression"
    return 0
  fi

  load_inline_variants <<'INI'
[griffon-test:chess]
customPiece1 = a:O
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 7k/8/8/8/3A4/8/8/K7 w - - 0 1

[manticore-test:chess]
customPiece1 = a:M
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 7k/8/8/8/3A4/8/8/K7 w - - 0 1
INI

  local g m
  g=$(run_uci "$ENGINE" "$TMP_VARIANTS" griffon-test <<'UCI'
position startpos
go perft 1
UCI
)
  assert_contains "$g" "d4h5:"
  assert_contains "$g" "d4a5:"
  assert_contains "$g" "d4e8:"
  assert_contains "$g" "d4c1:"
  assert_not_contains "$g" "d4d5:"
  assert_not_contains "$g" "d4e4:"

  m=$(run_uci "$ENGINE" "$TMP_VARIANTS" manticore-test <<'UCI'
position startpos
go perft 1
UCI
)
  assert_contains "$m" "d4g8:"
  assert_contains "$m" "d4a6:"
  assert_contains "$m" "d4h1:"
  assert_contains "$m" "d4b1:"
  assert_not_contains "$m" "d4h5:"
  assert_not_contains "$m" "d4e8:"
}

test_bent_rider_evasion() {
  if ! variant_available "$ENGINE" bent-rider-evasion "$VARIANTS"; then
    echo "bent-rider-evasion variant not available in this build; skipping bent-rider evasion regression"
    return 0
  fi

  load_inline_variants <<'INI'
[griffon-evasion:chess]
customPiece1 = a:O
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k

[manticore-evasion:chess]
customPiece1 = a:M
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
INI

  local g m
  g=$(run_uci "$ENGINE" "$TMP_VARIANTS" griffon-evasion <<'UCI'
position fen 6r1/7k/5A2/8/8/8/8/K7 b - - 0 1
go perft 1
UCI
)
  assert_contains "$g" "g8g7:"
  assert_not_contains "$g" "g8g6:"

  m=$(run_uci "$ENGINE" "$TMP_VARIANTS" manticore-evasion <<'UCI'
position fen 6k1/7r/5A2/8/8/8/8/K7 b - - 0 1
go perft 1
UCI
)
  assert_contains "$m" "h7f7:"
  assert_not_contains "$m" "h7h6:"
}

test_asym_rider_checkers() {
  if ! variant_available "$ENGINE" asym-rider-checkers "$VARIANTS"; then
    echo "asym-rider-checkers variant not available in this build; skipping asym rider checkers regression"
    return 0
  fi

  load_inline_variants <<'INI'
[asymcheck-horse:chess]
customPiece1 = a:nN
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k

[asymcheck-griffon:chess]
customPiece1 = a:O
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k

[asymcheck-manticore:chess]
customPiece1 = a:M
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
INI

  local hb hu gb gu gx mb mu
  hb=$(run_uci "$ENGINE" "$TMP_VARIANTS" asymcheck-horse <<'UCI'
position fen 4k3/3R4/3A4/8/8/8/8/4K3 b - - 0 1
d
UCI
)
  assert_contains "$hb" "^Checkers:[[:space:]]*$"

  hu=$(run_uci "$ENGINE" "$TMP_VARIANTS" asymcheck-horse <<'UCI'
position fen 4k3/8/3A4/8/8/8/8/4K3 b - - 0 1
d
UCI
)
  assert_contains "$hu" "^Checkers: d6 "

  gb=$(run_uci "$ENGINE" "$TMP_VARIANTS" asymcheck-griffon <<'UCI'
position fen 8/6Pk/5A2/8/8/8/8/4K3 b - - 0 1
d
UCI
)
  assert_contains "$gb" "^Checkers:[[:space:]]*$"

  gu=$(run_uci "$ENGINE" "$TMP_VARIANTS" asymcheck-griffon <<'UCI'
position fen 8/7k/5A2/8/8/8/8/4K3 b - - 0 1
d
UCI
)
  assert_contains "$gu" "^Checkers: f6 "

  gx=$(run_uci "$ENGINE" "$TMP_VARIANTS" asymcheck-griffon <<'UCI'
position fen 8/5P1k/5A2/8/8/8/8/4K3 b - - 0 1
d
UCI
)
  assert_contains "$gx" "^Checkers: f6 "

  mb=$(run_uci "$ENGINE" "$TMP_VARIANTS" asymcheck-manticore <<'UCI'
position fen 6k1/5N2/5A2/8/8/8/8/4K3 b - - 0 1
d
UCI
)
  assert_contains "$mb" "^Checkers:[[:space:]]*$"

  mu=$(run_uci "$ENGINE" "$TMP_VARIANTS" asymcheck-manticore <<'UCI'
position fen 6k1/8/5A2/8/8/8/8/4K3 b - - 0 1
d
UCI
)
  assert_contains "$mu" "^Checkers: f6 "
}

test_alfil_dabbaba_riders() {
  if ! variant_available "$ENGINE" alfil-dabbaba-riders "$VARIANTS"; then
    echo "alfil-dabbaba-riders variant not available in this build; skipping alfil/dabbaba regression"
    return 0
  fi

  load_inline_variants <<'INI'
[alfil-rider:chess]
customPiece1 = a:AA
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 6k1/8/8/8/3A4/8/8/K7 w - - 0 1

[alfil-rider-tuple:chess]
customPiece1 = a:(2,2)(2,2)
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 6k1/8/8/8/3A4/8/8/K7 w - - 0 1

[alfil-rider-tuple-blocked:chess]
customPiece1 = a:(2,2)(2,2)
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 6k1/8/5p2/8/3A4/8/8/K7 w - - 0 1

[dabbaba-rider:chess]
customPiece1 = a:DD
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 6k1/8/8/8/3A4/8/8/K7 w - - 0 1

[dabbaba-rider-tuple:chess]
customPiece1 = a:(2,0)2
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 6k1/8/8/8/3A4/8/8/K7 w - - 0 1

[tuple-range-pin:chess]
customPiece1 = a:(1,0)2
customPiece2 = b:W
pieceToCharTable = PNBRQ............AB..Kpnbrq............ab..k
startFen = 3a4/8/8/8/8/3B4/8/3K4 w - - 0 1

[lame-rider-blockers:chess]
customPiece1 = a:nD
customPiece2 = b:nDD
customPiece3 = c:nA
customPiece4 = d:nAA
pieceToCharTable = PNBRQ............ABCDKpnbrq............abcdk
startFen = 8/3ab3/2cd5/8/8/8/8/K6k b - - 0 1

[lame-rider-repeat:chess]
customPiece1 = a:nAA
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 8/3a4/8/8/8/8/8/K6k b - - 0 1

[lame-rider-bounded:chess]
customPiece1 = a:n{path:mid}A2
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 6k1/8/8/8/8/8/1A6/K7 w - - 0 1

[plain-rider-midpoint:chess]
customPiece1 = a:DD
customPiece2 = b:AA
pieceToCharTable = PNBRQ............AB..Kpnbrq............ab..k
startFen = 8/3ab3/3pp3/8/8/8/8/K6k b - - 0 1

[lame-path-orthfirst:chess]
customPiece1 = a:n{path:orthfirst}L
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A1p4K w - - 0 1

[lame-path-mid:chess]
customPiece1 = a:n{path:mid}L
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A1p4K w - - 0 1

[lame-path-mid-clear:chess]
customPiece1 = a:n{path:mid}L
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A6K w - - 0 1

[lame-ferz-blockers:chess]
customPiece1 = a:nF
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 8/8/8/8/3A4/8/8/K6k w - - 0 1

[moo-anypath:chess]
customPiece1 = a:n{path:anypath}N
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/pp6/A6K w - - 0 1

[moa-move-blocked:chess]
customPiece1 = a:n{path:diagfirst}N
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/1p5K/A7 w - - 0 1

[mao-leg-blocked:chess]
customPiece1 = a:n{path:orthfirst}N
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/Ap5K w - - 0 1

[mao-leg-clear:chess]
customPiece1 = a:n{path:orthfirst}N
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A1p4K w - - 0 1

[lame-filter-key-reject:chess]
customPiece1 = a:n{path:orthfirst;filter:first}L
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A6K w - - 0 1

[lame-filter-value-reject:chess]
customPiece1 = a:n{path:orthfirst;filter:last}L
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A6K w - - 0 1

[lame-invalid-clears-piece:chess]
customPiece1 = a:Rn{path:orthfirst;filter:first}L
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A6K w - - 0 1

[lame-invalid-stops-piece:chess]
customPiece1 = a:Rn{path:bad}LB
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A6K w - - 0 1

[lame-invalid-multi-block:chess]
customPiece1 = a:n{path:bad}{path:mid}N
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A6K w - - 0 1

[lame-invalid-dangling-path:chess]
customPiece1 = a:Rn{path:bad}
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A6K w - - 0 1

[lame-invalid-dangling-filter:chess]
customPiece1 = a:Rn{filter:first}
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A6K w - - 0 1

[lame-invalid-only-block:chess]
customPiece1 = a:n{path:bad}
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A6K w - - 0 1

[lame-valid-after-block:chess]
customPiece1 = a:Rn{path:mid}A
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A6K w - - 0 1

[lame-tuple-reject:chess]
customPiece1 = a:Rn(2,1)
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A6K w - - 0 1

[lame-path-mid-single:chess]
customPiece1 = a:n{path:mid}D
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/p7/A6K w - - 0 1

[lame-long-leaper:chess]
customPiece1 = a:n{path:anypath}U
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A6K w - - 0 1

[moa-check:chess]
customPiece1 = a:n{path:diagfirst}N
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 8/8/8/8/3k4/2p5/8/A6K w - - 0 1

[lame-range-reject:chess]
customPiece1 = a:n{path:mid}A[2-3]
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A6K w - - 0 1

[lame-bare-hopper-reject:chess]
customPiece1 = a:npW
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A6K w - - 0 1

[lame-bare-dynamic-reject:chess]
customPiece1 = a:nxR
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A6K w - - 0 1

[lame-bare-ski-reject:chess]
customPiece1 = a:njR
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A6K w - - 0 1

[lame-bare-max-reject:chess]
customPiece1 = a:nzR
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A6K w - - 0 1

[lame-hybrid-rook-check:chess]
customPiece1 = a:RnN
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A6K w - - 0 1

[cylinder-anypath:chess]
cylindrical = true
customPiece1 = a:n{path:anypath}N
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A3K3 w - - 0 1

[cylinder-orthfirst:chess]
cylindrical = true
customPiece1 = a:n{path:orthfirst}N
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A3K3 w - - 0 1

[lame-key-routing:chess]
customPiece1 = a:n{capture:dest;path:orthfirst}L
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k

[hopper-key-routing:chess]
customPiece1 = a:{path:orthfirst}W
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k

[hopper-capture-value-reject:chess]
customPiece1 = a:{path:orthfirst}W
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
INI

  local out
  out=$(run_uci "$ENGINE" "$TMP_VARIANTS" alfil-rider <<'UCI'
position startpos
go perft 1
UCI
)
  assert_contains "$out" "d4g7:"

  out=$(run_uci "$ENGINE" "$TMP_VARIANTS" alfil-rider-tuple <<'UCI'
position startpos
go perft 1
UCI
)
  assert_contains "$out" "d4g7:"

  out=$(run_uci "$ENGINE" "$TMP_VARIANTS" alfil-rider-tuple-blocked <<'UCI'
position startpos
go perft 1
UCI
)
  assert_not_contains "$out" "d4g7:"

  out=$(run_uci "$ENGINE" "$TMP_VARIANTS" dabbaba-rider <<'UCI'
position startpos
go perft 1
UCI
)
  assert_contains "$out" "d4d6:"

  out=$(run_uci "$ENGINE" "$TMP_VARIANTS" dabbaba-rider-tuple <<'UCI'
position startpos
go perft 1
UCI
)
  assert_contains "$out" "d4d6:"
}

test_rose
test_bent_riders
test_bent_rider_evasion
test_asym_rider_checkers
test_alfil_dabbaba_riders
