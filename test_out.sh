#!/bin/bash
set -euo pipefail

ENGINE="${1:-./src/stockfish}"
PWD_PATH=$(pwd)

cat << 'IN' > tests/temp_variants.ini
[walling-gating-test:chess]
wallingRule = duck
seirawanGating = true
IN

"$ENGINE" << IN
setoption name VariantPath value $PWD_PATH/tests/temp_variants.ini
setoption name UCI_Variant value walling-gating-test
isready
d
quit
IN
rm tests/temp_variants.ini
