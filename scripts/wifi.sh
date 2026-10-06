#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/config.sh"
"$PYTHON" "$REPO_ROOT/tools/wifi_setup.py" "${1:?Usage: scripts/wifi.sh SERIAL_PORT}"
