#!/bin/bash

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
export ROOT_DIR
source "${ROOT_DIR}/tests/lib/uci.sh"
setup_test_context "${1:-}" "${2:-}" "misc engine regressions"

test_invalid_scalar_regression() {
  local tmp_ini output
  tmp_ini=$(mktemp "${TMPDIR:-/tmp}/fsx-invalid-scalar-XXXXXX.ini")
  cat >"${tmp_ini}" <<'INI'
[bad-bool-scalar:chess]
chess960 = maybe
INI

  output=$("${ENGINE}" check "${tmp_ini}" 2>&1 || true)
  assert_contains_literal "$output" "Parsing variant: bad-bool-scalar"
  assert_contains_literal "$output" "chess960 - Invalid value maybe for type bool"
  rm -f "${tmp_ini}"
}

test_verbosity() {
  local uci_output quiet_output debug_output
  uci_output=$(run_uci "$ENGINE" "$VARIANTS" chess <<'EOF'
EOF
)
  assert_contains "$uci_output" 'option name Verbosity type spin default 1 min 0 max 2'

  quiet_output=$(run_uci "$ENGINE" "$VARIANTS" chess <<'EOF'
setoption name Verbosity value 0
position startpos
go depth 2
EOF
)
  if grep -q '^info depth ' <<<"$quiet_output"; then
    echo "Verbosity=0 unexpectedly emitted search info"
    return 1
  fi

  debug_output=$(run_uci "$ENGINE" "$VARIANTS" chess <<'EOF'
setoption name Verbosity value 2
position fen 7k/5Q2/7K/8/8/8/8/8 b - - 0 1
go depth 1
EOF
)
  assert_contains "$debug_output" 'info string adjudication reason stalemate result cp 0 side_to_move black'
}

test_rootmove_searchmoves() {
  local out best skill_out skill_best
  out=$(run_uci "$ENGINE" "$VARIANTS" chess <<'EOF'
setoption name MultiPV value 3
position startpos
go depth 4 searchmoves e2e4 d2d4 g1f3
EOF
)
  best="$(printf '%s\n' "$out" | awk '/^bestmove / { print $2 }')"
  [[ "$best" =~ ^(e2e4|d2d4|g1f3)$ ]]

  mapfile -t pvs < <(printf '%s\n' "$out" | awk '/ multipv / { for (i = 1; i <= NF; ++i) if ($i == "pv") { print $(i + 1); break } }' | sort -u)
  for mv in "${pvs[@]}"; do
    [[ "$mv" =~ ^(e2e4|d2d4|g1f3)$ ]]
  done

  skill_out=$(run_uci "$ENGINE" "$VARIANTS" chess <<'EOF'
setoption name Skill Level value 0
position startpos
go depth 4 searchmoves e2e4 d2d4 g1f3 b1c3
EOF
)
  skill_best="$(printf '%s\n' "$skill_out" | awk '/^bestmove / { print $2 }')"
  [[ "$skill_best" =~ ^(e2e4|d2d4|g1f3|b1c3)$ ]]
}

test_gating_regressions() {
  load_inline_variants <<'EOF'
[gatingblock:chess]
gating = true
seirawanGating = true

[symgating:chess]
gating = true
seirawanGating = true
symmetricDropTypes = r
EOF
  local tmp_ini="${FSX_TMP_INI}" out

  out=$(run_uci "$ENGINE" "$tmp_ini" gatingblock <<<'position fen 8/8/8/8/8/8/8/R3K2k[N] w KQBCDEFGH - 0 1 moves e1e2n
d')
  assert_contains "$out" "Fen: 8/8/8/8/8/8/4K3/R3N2k\\[\\] b - - 1 1"
  assert_contains "$out" "^Checkers: *$"

  out=$(run_uci "$ENGINE" "$tmp_ini" symgating <<<'position fen 4k3/8/8/8/8/8/8/4K3[RR] w ABCDEFGH - 0 1
go perft 1')
  assert_contains "$out" "^e1d1: 1$"
  assert_not_contains "$out" "^e1d1r,d1: 1$"
  assert_contains "$out" "^e1e2r,d1: 1$"
  assert_nodes "$out" 9

  out=$(run_uci "$ENGINE" "$tmp_ini" symgating <<<'position fen 4k3/8/8/8/8/8/8/4K3[RR] w ABCDEFGH - 0 1 moves e1e2r,d1
d')
  assert_contains "$out" "Fen: 4k3/8/8/8/8/8/4K3/3RR3\\[\\] b - - 1 1"
}

test_bench_stdin() {
  local baseline_output baseline_nodes output nodes
  baseline_output="$(run_engine_stdin "$ENGINE" $'uci\nbench 16 1 1 default depth\nquit\n')"
  baseline_nodes="$(printf '%s\n' "$baseline_output" | bench_nodes)"
  if [[ -z "${baseline_nodes}" ]]; then
    printf '%s\n' "$baseline_output"
    echo "bench stdin regression failed to produce baseline node count"
    return 1
  fi

  output="$(run_engine_stdin "$ENGINE" $'uci\nsetoption name Threads value 4\nsetoption name Hash value 32\nbench 0 0 1 default depth\nquit\n')"
  nodes="$(printf '%s\n' "$output" | bench_nodes)"
  if [[ -z "${nodes}" ]]; then
    printf '%s\n' "$output"
    echo "bench stdin regression failed to produce node count"
    return 1
  fi
}

test_signature() {
  local bench_output signature
  bench_output="$(UCI_TIMEOUT=15m run_engine_stdin "$ENGINE" $'bench\nquit\n')"
  signature="$(printf '%s\n' "$bench_output" | bench_nodes)"

  if [[ -z "${signature}" ]]; then
    echo "No signature obtained from bench. Code crashed or assert triggered ?"
    printf '%s\n' "$bench_output"
    return 1
  fi

  echo "$signature"
}

test_invalid_scalar_regression
test_verbosity
test_rootmove_searchmoves
test_gating_regressions
test_bench_stdin
test_signature
