#!/usr/bin/env bash
set -euo pipefail
exec python3 -B "$(dirname "$0")/setup-dependencies.py" "$@"
