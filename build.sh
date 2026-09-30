#!/usr/bin/env bash
# Compile the sketch with arduino-cli. Works from WSL or Linux.
# Usage: ./build.sh [extra arduino-cli flags, e.g. --clean]
set -euo pipefail
cd "$(dirname "$0")"

ACLI="${ARDUINO_CLI:-$HOME/.local/bin/arduino-cli}"
command -v "$ACLI" >/dev/null 2>&1 || ACLI="$(command -v arduino-cli)"
# Default: Nano (ATmega328P, new bootloader). If upload fails with sync errors,
# rebuild with: ./build.sh OLD_BOOTLOADER=1  (course clone boards often need it)
FQBN="${FQBN:-arduino:avr:nano}"
if [ "${OLD_BOOTLOADER:-0}" = "1" ]; then FQBN="arduino:avr:nano:cpu=atmega328old"; fi

mkdir -p build
"$ACLI" compile --fqbn "$FQBN" --output-dir build sketch/hovercraft_ta1 "$@"
echo "OK: build/hovercraft_ta1.ino.elf + build/hovercraft_ta1.ino.hex"
