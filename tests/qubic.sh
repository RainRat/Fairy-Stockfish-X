#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "${BASH_SOURCE[0]}")/lib/uci.sh"

ENGINE=$(default_engine "${1:-}")
VARIANTS=$(default_variants "${2:-}")

variant_available() {
  probe_variant_available "$ENGINE" qubic "$VARIANTS"
}

echo "qubic regression tests started"

if ! variant_available; then
  echo "qubic regression skipped: variant unavailable in this build"
  exit 0
fi

out=$(run_uci "$ENGINE" "$VARIANTS" qubic <<<'position fen 8/8/8/8/8/8/8/8[pppppppppppppppppppppppppppppppp] b - - 0 1
go perft 1')
assert_nodes "$out" 64

out=$(run_uci "$ENGINE" "$VARIANTS" qubic <<<'position fen 8/8/8/P3P3/8/8/8/P3P3[pppppppppppppppppppppppppppp] b - - 0 1
go perft 1')
assert_nodes "$out" 0

out=$(run_uci "$ENGINE" "$VARIANTS" qubic <<<'position fen 8/8/8/8/8/8/8/P7[ppppppppppppppppppppppppppppppp] b - - 0 1
go perft 1')
assert_nodes "$out" 63

out=$(run_uci "$ENGINE" "$VARIANTS" qubic <<<'position fen 7P/2P5/8/8/8/8/5P2/P7[pppppppppppppppppppppppppppp] b - - 0 1
go perft 1')
assert_nodes "$out" 0

echo "qubic regression tests passed"
