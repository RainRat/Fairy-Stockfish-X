#!/bin/bash
set -euo pipefail

ENGINE="${1:-./src/stockfish}"
PWD_PATH=$(pwd)

"$ENGINE" << IN
setoption name VariantPath value $PWD_PATH/src/variants.ini
isready
setoption name UCI_Variant value walling-gating-test
isready
d
quit
IN
