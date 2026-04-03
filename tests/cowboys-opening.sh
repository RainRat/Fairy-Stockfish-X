#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
ENGINE="${1:-${REPO_ROOT}/src/stockfish}"
VARIANTS="${2:-${REPO_ROOT}/src/variants.ini}"

tmp_exp="$(mktemp)"
trap 'rm -f "$tmp_exp"' EXIT

cat >"$tmp_exp" <<'EXP'
#!/usr/bin/expect -f
set timeout 20
set engine [lindex $argv 0]
set variant_path [lindex $argv 1]

spawn $engine
send "uci\r"
expect "uciok"
send "setoption name VariantPath value $variant_path\r"
send "setoption name UCI_Variant value cowboys\r"
send "setoption name MultiPV value 6\r"
send "isready\r"
expect "readyok"
send "position startpos\r"
send "go depth 7\r"
expect {
  -re "bestmove\\s+\\S+" { exit 0 }
  eof { exit 1 }
  timeout { exit 2 }
}
EXP

expect "$tmp_exp" "$ENGINE" "$VARIANTS"
