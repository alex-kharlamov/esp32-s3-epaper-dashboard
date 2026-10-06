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
"$CXX" "${FLAGS[@]}" -I "$SCENE" "$REPO_ROOT/tools/preview.cpp" "$SCENE/Dashboard.cpp" "$SCENE/DashboardAssets.cpp" "$SCENE/SampleData.cpp" -o "$OUT/renderer"
"$OUT/renderer" "$OUT/frame.bin"
"$PYTHON" - "$OUT/frame.bin" "$OUT/dashboard.png" <<'PY'
import sys
from PIL import Image
raw=open(sys.argv[1],'rb').read()
assert len(raw)==1360*480//4
palette=[(0,0,0),(255,255,255),(255,213,0),(200,25,32)]
im=Image.new('RGB',(1360,480))
im.putdata([palette[(b>>s)&3] for b in raw for s in (6,4,2,0)])
im.save(sys.argv[2]);print('Preview:',sys.argv[2])
PY
