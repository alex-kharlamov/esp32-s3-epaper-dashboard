#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/../scripts/config.sh"
SCENE="$REPO_ROOT/ESP32S3_Dashboard"
OUT="$REPO_ROOT/build/preview"
mkdir -p "$OUT"
CXX="${CXX:-c++}"
FLAGS=(-std=c++17)
if [[ "$(uname -s)" == Darwin ]]; then
  if [[ -x /Library/Developer/CommandLineTools/usr/bin/clang++ ]]; then
    SDK=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk
    CXX="${CXX_OVERRIDE:-/Library/Developer/CommandLineTools/usr/bin/clang++}"
  else
    SDK="$(xcrun --sdk macosx --show-sdk-path)"
    CXX="${CXX_OVERRIDE:-$(xcrun --find clang++)}"
  fi
  FLAGS+=(-isysroot "$SDK" "-Wl,-syslibroot,$SDK")
fi
"$CXX" "${FLAGS[@]}" -O2 -I "$SCENE" "$REPO_ROOT/tools/clock_windows_test.cpp" "$SCENE/Dashboard.cpp" "$SCENE/DashboardAssets.cpp" "$SCENE/SleepArtwork.cpp" "$SCENE/SampleData.cpp" -o "$OUT/clock-windows-test"
"$OUT/clock-windows-test"
