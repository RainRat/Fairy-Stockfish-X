#!/bin/bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

ENGINE=$(default_engine "${1:-}")

tmp_ini=$(mktemp)
trap 'rm -f "$tmp_ini"' EXIT

cat > "$tmp_ini" <<'INI'
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

diag() {
  local variant=$1
  local fen=$2
  run_uci "$ENGINE" "$tmp_ini" "$variant" <<UCI
position fen $fen
d
UCI
}

# Horse-family blocked leg: no check.
hb=$(diag asymcheck-horse '4k3/3R4/3A4/8/8/8/8/4K3 b - - 0 1')
assert_contains "$hb" "^Checkers:[[:space:]]*$"

# Horse-family open leg: checker on d6.
hu=$(diag asymcheck-horse '4k3/8/3A4/8/8/8/8/4K3 b - - 0 1')
assert_contains "$hu" "^Checkers: d6 "

# Griffon blocked pivot: no check.
gb=$(diag asymcheck-griffon '8/6Pk/5A2/8/8/8/8/4K3 b - - 0 1')
assert_contains "$gb" "^Checkers:[[:space:]]*$"

# Griffon open pivot/ray: checker on f6.
gu=$(diag asymcheck-griffon '8/7k/5A2/8/8/8/8/4K3 b - - 0 1')
assert_contains "$gu" "^Checkers: f6 "

# Non-pivot orthogonal blocker must not suppress the same griffon check.
gx=$(diag asymcheck-griffon '8/5P1k/5A2/8/8/8/8/4K3 b - - 0 1')
assert_contains "$gx" "^Checkers: f6 "

# Manticore blocked pivot: no check.
mb=$(diag asymcheck-manticore '6k1/5N2/5A2/8/8/8/8/4K3 b - - 0 1')
assert_contains "$mb" "^Checkers:[[:space:]]*$"

# Manticore open pivot/ray: checker on f6.
mu=$(diag asymcheck-manticore '6k1/8/5A2/8/8/8/8/4K3 b - - 0 1')
assert_contains "$mu" "^Checkers: f6 "

echo "asym-rider-checkers test OK"
