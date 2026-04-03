#!/bin/bash
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
ENGINE="${1:-${REPO_ROOT}/src/stockfish}"
set -euo pipefail

# cd "$(dirname "$0")/../src" # removed for absolute paths

tmp_ini=$(mktemp)
trap 'rm -f "$tmp_ini"' EXIT

cat > "$tmp_ini" <<'INI'
[alias-wazir:chess]
customPiece1 = a:wazir
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 7k/8/8/8/3A4/8/8/K7 w - - 0 1

[alias-wazir-ref:chess]
customPiece1 = a:W
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 7k/8/8/8/3A4/8/8/K7 w - - 0 1

[alias-nightrider:chess]
customPiece1 = a:nightrider
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 7k/8/8/8/3A4/8/8/K7 w - - 0 1

[alias-nightrider-ref:chess]
customPiece1 = a:NN
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 7k/8/8/8/3A4/8/8/K7 w - - 0 1

[alias-grasshopper:chess]
customPiece1 = a:grasshopper
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 7k/8/8/8/3A4/8/8/K7 w - - 0 1

[alias-grasshopper-ref:chess]
customPiece1 = a:gQ
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 7k/8/8/8/3A4/8/8/K7 w - - 0 1

[alias-marshall:chess]
customPiece1 = a:marshall
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 7k/8/8/8/3A4/8/8/K7 w - - 0 1

[alias-marshall-ref:chess]
customPiece1 = a:RN
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 7k/8/8/8/3A4/8/8/K7 w - - 0 1

[alias-cardinal:chess]
customPiece1 = a:cardinal
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 7k/8/8/8/3A4/8/8/K7 w - - 0 1

[alias-cardinal-ref:chess]
customPiece1 = a:BN
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 7k/8/8/8/3A4/8/8/K7 w - - 0 1
INI

perft_moves() {
  local variant=$1
  printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value %s\nposition startpos\ngo perft 1\nquit\n' "$tmp_ini" "$variant" \
    | "$ENGINE" \
    | grep -E '^[a-z][0-9]+[a-z][0-9]+:'
}

cmp <(perft_moves alias-wazir) <(perft_moves alias-wazir-ref)
cmp <(perft_moves alias-nightrider) <(perft_moves alias-nightrider-ref)
cmp <(perft_moves alias-grasshopper) <(perft_moves alias-grasshopper-ref)
cmp <(perft_moves alias-marshall) <(perft_moves alias-marshall-ref)
cmp <(perft_moves alias-cardinal) <(perft_moves alias-cardinal-ref)

echo "common-fairy-aliases test OK"