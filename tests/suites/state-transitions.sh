#!/usr/bin/env bash
exec "$(cd "$(dirname "$0")/.." && pwd)/lib/suite.sh" state-transitions "$@"
