#!/bin/bash
# verify protocol implementations

set -euo pipefail

error()
{
  echo "protocol testing failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

echo "protocol testing started"

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
ENGINE="${1:-${REPO_ROOT}/src/stockfish}"

uci_exp=$(mktemp)
ucci_exp=$(mktemp)
usi_exp=$(mktemp)
ucicyclone_exp=$(mktemp)
ucicyclone2_exp=$(mktemp)
xboard_exp=$(mktemp)
trap 'rm -f "$uci_exp" "$ucci_exp" "$usi_exp" "$ucicyclone_exp" "$ucicyclone2_exp" "$xboard_exp"' EXIT

cat << EOF > "$uci_exp"
   set engine [lindex \$argv 0]
   spawn \$engine
   send "uci\\n"
   expect "default chess"
   expect "uciok"
   send "quit\\n"
   expect eof
EOF

cat << EOF > "$ucci_exp"
   set engine [lindex \$argv 0]
   spawn \$engine
   send "ucci\\n"
   expect "option UCI_Variant"
   expect -re "default (xiangqi|minixiangqi)"
   expect "ucciok"
   send "quit\\n"
   expect eof
EOF

cat << EOF > "$usi_exp"
   set engine [lindex \$argv 0]
   spawn \$engine
   send "usi\\n"
   expect -re "default (shogi|minishogi)"
   expect "usiok"
   send "quit\\n"
   expect eof
EOF

cat << EOF > "$ucicyclone_exp"
   set engine [lindex \$argv 0]
   spawn \$engine
   send "uci\\n"
   expect "uciok"
   send "startpos\\n"
   send "d\\n"
   expect -re "(rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1|rnbakabnr/9/1c5c1/p1p1p1p1p/9/9/P1P1P1P1P/1C5C1/9/RNBAKABNR w - - 0 1)"
   send "quit\\n"
   expect eof
EOF

cat << EOF > "$ucicyclone2_exp"
   set engine [lindex \$argv 0]
   spawn \$engine ucicyclone
   send "uci\\n"
   expect "uciok"
   send "position startpos\\n"
   send "d\\n"
   expect -re "(rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1|rcnkncr/p1ppp1p/7/7/7/P1PPP1P/RCNKNCR w - - 0 1|rnbakabnr/9/1c5c1/p1p1p1p1p/9/9/P1P1P1P1P/1C5C1/9/RNBAKABNR w - - 0 1)"
   send "quit\\n"
   expect eof
EOF

cat << EOF > "$xboard_exp"
   set engine [lindex \$argv 0]
   set variant_path [lindex \$argv 1]
   spawn \$engine load \$variant_path
   send "xboard\\n"
   send "protover 2\\n"
   expect "feature done=1"
   send "ping\\n"
   expect "pong"
   send "ping\\n"
   expect "pong"
   send "variant 3check-crazyhouse\\n"
   expect {rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR[] w KQkq - 3+3 0 1}
   send "quit\\n"
   expect eof
EOF

for exp in "$uci_exp" "$ucci_exp" "$usi_exp" "$ucicyclone_exp" "$ucicyclone2_exp" "$xboard_exp"
do
  echo "Testing $exp"
  timeout 20 expect "$exp" "$ENGINE" "${REPO_ROOT}/src/variants.ini" > /dev/null
done

echo "protocol testing OK"
