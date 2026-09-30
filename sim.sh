#!/usr/bin/env bash
# Run the firmware in the Wokwi simulator (headless). Requires WOKWI_CLI_TOKEN
# (free account: wokwi.com -> Dashboard -> CI) or use the VS Code extension instead.
# Usage: ./sim.sh [timeout_ms]   (default 12000)
set -euo pipefail
cd "$(dirname "$0")"
[ -f build/hovercraft_ta1.ino.elf ] || { echo "Run ./build.sh first"; exit 1; }
WOKWI="${WOKWI_CLI:-$HOME/.local/bin/wokwi-cli}"
command -v "$WOKWI" >/dev/null 2>&1 || WOKWI="$(command -v wokwi-cli)"
"$WOKWI" --timeout "${1:-12000}" --expect-text ';100;' --serial-log-file build/serial.log .
echo "Sim passed - serial log: build/serial.log"
