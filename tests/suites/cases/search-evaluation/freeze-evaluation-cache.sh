#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/../../../../tests/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "freeze evaluation cache"

load_inline_variants <<'INI'
[freeze-eval-control:chess]
customPiece1 = x:mW
castling = false

[freeze-eval-active:freeze-eval-control]
freezePieceTypes = x
INI

white_mobility_mg() {
  awk -F'|' '
    $2 ~ /Mobility/ {
      count = split($3, scores, /[[:space:]]+/)
      for (i = 1; i <= count; ++i)
        if (scores[i] ~ /^-?[0-9]+([.][0-9]+)?$/) {
          print scores[i]
          exit
        }
    }
  '
}

eval_trace() {
  local variant="$1"
  run_uci "${ENGINE}" "${FSX_TMP_INI}" "${variant}" <<'UCI'
setoption name Use NNUE value false
position fen 7k/8/8/4x3/4N3/8/8/7K w - - 0 1
eval
UCI
}

# The black x on e5 freezes the white knight on e4 only in the active variant.
# Its white mobility score must fall when Evaluation consumes the cached mask.
control_trace=$(eval_trace freeze-eval-control)
frozen_trace=$(eval_trace freeze-eval-active)
control_mobility=$(white_mobility_mg <<<"${control_trace}")
frozen_mobility=$(white_mobility_mg <<<"${frozen_trace}")

[[ -n "${control_mobility}" && -n "${frozen_mobility}" ]] || {
  echo "could not read white mobility score from evaluation trace" >&2
  exit 1
}

if ! awk -v control="${control_mobility}" -v frozen="${frozen_mobility}" \
  'BEGIN { exit !(frozen < control) }'; then
  echo "freezing the white knight did not reduce its mobility evaluation" >&2
  echo "control=${control_mobility}, frozen=${frozen_mobility}" >&2
  exit 1
fi

echo "freeze evaluation cache regression passed"
