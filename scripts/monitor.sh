#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/config.sh"
"$PYTHON" "$REPO_ROOT/tools/monitor.py" "$@"
