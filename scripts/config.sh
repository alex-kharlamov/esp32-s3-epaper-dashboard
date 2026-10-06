#!/usr/bin/env bash
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TOOLS_DIR="${DASHBOARD_TOOLS_DIR:-$REPO_ROOT/.tools}"
CLI="${ARDUINO_CLI:-$TOOLS_DIR/bin/arduino-cli}"
CLI_CONFIG="${ARDUINO_CLI_CONFIG:-$TOOLS_DIR/arduino-cli.yaml}"
PYTHON="${DASHBOARD_PYTHON:-$TOOLS_DIR/venv/bin/python}"
BUILD_DIR="$REPO_ROOT/build/firmware"
FQBN='esp32:esp32:esp32s3:FlashSize=16M,FlashMode=qio,PSRAM=opi,CDCOnBoot=cdc,USBMode=hwcdc,UploadMode=default,PartitionScheme=app3M_fat9M_16MB,UploadSpeed=460800,EraseFlash=none'
