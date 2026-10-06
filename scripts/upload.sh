#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/config.sh"
PORT="${1:?Usage: scripts/upload.sh SERIAL_PORT}"
mkdir -p "$REPO_ROOT/build"
# Identify hardware before touching flash. Binary offsets are supplied by Arduino CLI.
"$PYTHON" -m esptool --chip esp32s3 --port "$PORT" flash-id | tee "$REPO_ROOT/build/device.log"
if ! grep -q 'Detected flash size: 16MB' "$REPO_ROOT/build/device.log"; then
  echo 'Expected the tested 16 MB flash board. Adjust the profile for different hardware.' >&2; exit 1
fi
"$CLI" upload --config-file "$CLI_CONFIG" --fqbn "$FQBN" --port "$PORT" --input-dir "$BUILD_DIR" "$REPO_ROOT/ESP32S3_Dashboard"
