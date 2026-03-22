#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)

error() {
  echo "achi regression failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

export FSX_REPO_ROOT="${REPO_ROOT}"

python3 - <<'PY'
import os
import sys
repo_root = os.environ['FSX_REPO_ROOT']
sys.path.insert(0, repo_root)
import pyffish as sf

with open(os.path.join(repo_root, 'src', 'variants.ini'), 'r', encoding='utf-8') as f:
    cfg = f.read()
sf.load_variant_config(cfg)

center = sf.legal_moves('achi', '3/1P1/3 w - - 0 1', [])
assert sorted(center) == sorted(['b2a1', 'b2b1', 'b2c1', 'b2a2', 'b2c2', 'b2a3', 'b2b3', 'b2c3']), center

edge = sf.legal_moves('achi', '3/3/1P1 w - - 0 1', [])
assert sorted(edge) == sorted(['b1a1', 'b1c1', 'b1b2']), edge

corner = sf.legal_moves('achi', '3/3/P2 w - - 0 1', [])
assert sorted(corner) == sorted(['a1b1', 'a1a2', 'a1b2']), corner

blocked_by_enemy = sf.legal_moves('achi', '3/1Pp/3 w - - 0 1', [])
assert 'b2c2' not in blocked_by_enemy, blocked_by_enemy
assert sorted(blocked_by_enemy) == sorted(['b2a1', 'b2b1', 'b2c1', 'b2a2', 'b2a3', 'b2b3', 'b2c3']), blocked_by_enemy

print('achi regression tests passed')
PY
