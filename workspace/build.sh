#!/usr/bin/env bash
set -euo pipefail
# Flags are intentional: the old positional flattened-checkout interface is retired.
exec python3 -B "$(dirname "$0")/build.py" "$@"
