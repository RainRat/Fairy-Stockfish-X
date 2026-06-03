#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "same-player-board-repetition regression"

load_inline_variants <<'INI'
[same-player-repeat-control:chess]
startFen = 4k3/8/8/8/8/8/R7/4K3 w - - 0 1

[same-player-repeat-illegal:same-player-repeat-control]
samePlayerBoardRepetitionIllegal = true
INI
tmp_ini="${FSX_TMP_INI}"

run_perft() {
  local variant="$1"
  local moves="$2"
  run_uci "$ENGINE" "$tmp_ini" "$variant" <<CMDS
position startpos moves ${moves}
go perft 1
CMDS
}

echo "same-player-board-repetition regression tests started"

moves="a2a3 e8e7 a3a2 e7e8"

out=$(run_perft "same-player-repeat-control" "${moves}")
echo "${out}" | grep -q "^a2a3: 1$"

out=$(run_perft "same-player-repeat-illegal" "${moves}")
! echo "${out}" | grep -q "^a2a3: 1$"
echo "${out}" | grep -q "^e1d1: 1$"

echo "same-player-board-repetition regression tests passed"
