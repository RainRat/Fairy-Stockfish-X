#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/../../../../tests/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "blast pattern"

load_inline_variants <<'INI'
[blast-pattern-knight:chess]
blastOnCapture = true
blastPattern = N*

[blast-pattern-knight-no-center:blast-pattern-knight]
blastPattern = N

[blast-pattern-tuple:blast-pattern-knight]
blastPattern = ( 2, 1 )*

[blast-pattern-king:chess]
blastOnCapture = true
blastPattern = K*

[blast-pattern-legacy-king:chess]
blastOnCapture = true

[blast-pattern-zero-rifle:chess]
blastOnCapture = true
blastPattern = *
rifleCapture = true

[blast-pattern-parent:chess]
blastOnCapture = true
blastPattern = N*

[blast-pattern-inherited:blast-pattern-parent]

[blast-pattern-legacy-reset:blast-pattern-parent]
blastPattern = -
blastOrthogonals = false
blastDiagonals = false
blastCenter = true

[blast-pattern-bad-rider:chess]
blastPattern = R*

[blast-pattern-bad-direction:chess]
blastPattern = fN*

[blast-pattern-bad-mixed:chess]
blastPattern = N*
blastCenter = false

[blast-pattern-bad-inherited:blast-pattern-parent]
blastCenter = false
INI

tmp_ini="${FSX_TMP_INI}"
knight_fen="7k/8/8/2bp4/1n1R4/8/8/K7 w - - 0 1"

run_capture() {
  local variant="$1"
  run_uci "$ENGINE" "$tmp_ini" "$variant" <<UCI
position fen ${knight_fen} moves d4d5
d
UCI
}

out=$(run_capture blast-pattern-knight)
assert_contains "$out" "Fen: 7k/8/8/2b5/8/8/8/K7 b - - 0 1"

out=$(run_capture blast-pattern-knight-no-center)
assert_contains "$out" "Fen: 7k/8/8/2bR4/8/8/8/K7 b - - 0 1"

out=$(run_uci "$ENGINE" "$tmp_ini" blast-pattern-knight-no-center <<'UCI'
position fen 7k/8/b7/8/2n5/p7/R7/K7 w - - 0 1 moves a2a3
d
UCI
)
assert_contains "$out" "Fen: 7k/8/b7/8/8/R7/8/K7 b - - 0 1"

tuple_out=$(run_capture blast-pattern-tuple)
assert_contains "$tuple_out" "Fen: 7k/8/8/2b5/8/8/8/K7 b - - 0 1"

inherited_out=$(run_capture blast-pattern-inherited)
assert_contains "$inherited_out" "Fen: 7k/8/8/2b5/8/8/8/K7 b - - 0 1"

legacy_out=$(run_capture blast-pattern-legacy-king)
pattern_out=$(run_capture blast-pattern-king)
legacy_fen=$(grep "Fen:" <<<"$legacy_out")
pattern_fen=$(grep "Fen:" <<<"$pattern_out")
[[ "$legacy_fen" == "$pattern_fen" ]]

out=$(run_uci "$ENGINE" "$tmp_ini" blast-pattern-zero-rifle <<'UCI'
position fen r3k3/8/8/8/8/8/8/R3K3 w - - 0 1 moves a1a8
d
UCI
)
assert_contains "$out" "Fen: 4k3/8/8/8/8/8/8/R3K3 b"

out=$(run_capture blast-pattern-legacy-reset)
assert_contains "$out" "Fen: 7k/8/8/2b5/1n6/8/8/K7 b - - 0 1"

for variant in blast-pattern-bad-rider blast-pattern-bad-direction blast-pattern-bad-mixed blast-pattern-bad-inherited; do
  out=$(run_uci "$ENGINE" "$tmp_ini" "$variant" <<'UCI'
d
UCI
  )
  assert_contains "$out" "unknown variant '${variant}'; keeping 'chess'"
done

echo "blast pattern tests passed"
