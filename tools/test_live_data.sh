#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/../scripts/config.sh"
SCENE="$REPO_ROOT/ESP32S3_Dashboard"
OUT="$REPO_ROOT/build/tests"
mkdir -p "$OUT"
JSON_INCLUDE="${ARDUINOJSON_INCLUDE:-$TOOLS_DIR/arduino-user/libraries/ArduinoJson/src}"
CXX="${CXX:-c++}"
FLAGS=(-std=c++17 -O1 -g -fsanitize=address,undefined)
if [[ "$(uname -s)" == Darwin ]]; then
  SDK=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk
  CXX=/Library/Developer/CommandLineTools/usr/bin/clang++
  FLAGS+=(-isysroot "$SDK" "-Wl,-syslibroot,$SDK")
fi
"$CXX" "${FLAGS[@]}" -I "$SCENE" -I "$JSON_INCLUDE" "$REPO_ROOT/tools/live_data_test.cpp" "$SCENE/Dashboard.cpp" "$SCENE/DashboardAssets.cpp" "$SCENE/SleepArtwork.cpp" "$SCENE/SampleData.cpp" -o "$OUT/live-data-test"
"$OUT/live-data-test" "$REPO_ROOT/tools/fixtures/weather.json" "$REPO_ROOT/tools/fixtures/tfl-lines.json" "$REPO_ROOT/tools/fixtures/tfl-stations.json"
