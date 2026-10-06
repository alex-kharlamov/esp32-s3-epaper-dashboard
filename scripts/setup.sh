#!/usr/bin/env bash
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TOOLS_DIR="${DASHBOARD_TOOLS_DIR:-$REPO_ROOT/.tools}"
CLI_VERSION=1.5.2-rc.1
case "$(uname -s)/$(uname -m)" in
  Darwin/arm64) PLATFORM=macOS_ARM64 ;;
  Darwin/x86_64) PLATFORM=macOS_64bit ;;
  Linux/x86_64) PLATFORM=Linux_64bit ;;
  Linux/aarch64|Linux/arm64) PLATFORM=Linux_ARM64 ;;
  *) echo 'Unsupported setup platform. Use Arduino IDE; see docs/SETUP.md.' >&2; exit 1 ;;
esac
mkdir -p "$TOOLS_DIR/bin" "$TOOLS_DIR/data" "$TOOLS_DIR/downloads" "$TOOLS_DIR/user"
TOOLS_DIR="$(cd "$TOOLS_DIR" && pwd)"
curl -fLsS "https://downloads.arduino.cc/arduino-cli/arduino-cli_${CLI_VERSION}_${PLATFORM}.tar.gz" -o "$TOOLS_DIR/cli.tar.gz"
tar -xzf "$TOOLS_DIR/cli.tar.gz" -C "$TOOLS_DIR/bin"
cat > "$TOOLS_DIR/arduino-cli.yaml" <<YAML
board_manager:
  additional_urls:
    - https://espressif.github.io/arduino-esp32/package_esp32_index.json
directories:
  data: $TOOLS_DIR/data
  downloads: $TOOLS_DIR/downloads
  user: $TOOLS_DIR/user
YAML
CLI="$TOOLS_DIR/bin/arduino-cli"
"$CLI" core update-index --config-file "$TOOLS_DIR/arduino-cli.yaml"
"$CLI" core install esp32:esp32@3.3.12 --config-file "$TOOLS_DIR/arduino-cli.yaml"
"$CLI" lib install ArduinoJson@7.4.2 --config-file "$TOOLS_DIR/arduino-cli.yaml"
# The bundled Arduino ctags tool is Intel-only on the tested Apple Silicon host.
# Rebuild the same official release locally rather than requiring Rosetta.
if [[ "$(uname -s)/$(uname -m)" == Darwin/arm64 ]]; then
  curl -fLsS https://github.com/arduino/ctags/releases/download/5.8-arduino11/ctags-5.8-arduino11.tar.xz -o "$TOOLS_DIR/ctags.tar.xz"
  mkdir -p "$TOOLS_DIR/ctags-source"
  tar -xf "$TOOLS_DIR/ctags.tar.xz" -C "$TOOLS_DIR/ctags-source" --strip-components=1
  python3 - "$TOOLS_DIR/ctags-source" <<'PY'
import sys
from pathlib import Path
for p in Path(sys.argv[1]).glob('*'):
    if p.suffix in ('.c','.h'):
        p.write_bytes(p.read_bytes().replace(b'__unused__',b'CTAGS_UNUSED'))
PY
  if [[ -x /Library/Developer/CommandLineTools/usr/bin/clang ]]; then
    SDK=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk
    CTAGS_CC=/Library/Developer/CommandLineTools/usr/bin/clang
  else
    SDK="$(xcrun --sdk macosx --show-sdk-path)"
    CTAGS_CC="$(xcrun --find clang)"
  fi
  (cd "$TOOLS_DIR/ctags-source"
   CC="$CTAGS_CC" CPPFLAGS="-isysroot $SDK" CFLAGS="-isysroot $SDK" LDFLAGS="-Wl,-syslibroot,$SDK" ./configure
   make -j2
   cp ctags "$TOOLS_DIR/data/packages/builtin/tools/ctags/5.8-arduino11/ctags")
fi
python3 -m venv "$TOOLS_DIR/venv"
"$TOOLS_DIR/venv/bin/python" -m pip install -r "$REPO_ROOT/requirements.txt"
echo 'Ready: edit ESP32S3_Dashboard/Configuration.h, then run scripts/build.sh.'
