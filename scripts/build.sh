#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/config.sh"
mkdir -p "$REPO_ROOT/build"
"$CLI" compile --config-file "$CLI_CONFIG" --fqbn "$FQBN" --build-path "$BUILD_DIR" "$REPO_ROOT/ESP32S3_Dashboard" 2>&1 | tee "$REPO_ROOT/build/compile.log"
