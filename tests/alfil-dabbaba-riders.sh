#!/bin/bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "alfil dabbaba riders test"

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
customPiece1 = a:{capture:bogus}W
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k

[hopper-equi-value-reject:chess]
customPiece1 = a:{equi:bogus}W
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
INI
tmp_ini="${FSX_TMP_INI}"

piece_moves() {
  local variant=$1
  run_uci "$ENGINE" "$tmp_ini" "$variant" <<'UCI' | awk -F: '/^d4/{print $1}' | sort
position startpos
go perft 1
UCI
}

diff -u <(cat <<'EOF'
d4b2
d4b6
d4f2
d4f6
d4h8
EOF
) <(piece_moves alfil-rider)

diff -u <(cat <<'EOF'
d4b2
d4b6
d4f2
d4f6
EOF
) <(piece_moves alfil-rider-tuple-blocked)
