#!/usr/bin/env bash
# Bake the current firmware build into the installer image.
# Usage: ./build.sh [tag]        (default tag: cyds3-web-installer)
# Prereq: the firmware has been built (firmware/.pio/build/fnk0104s/firmware-merged.bin)
set -euo pipefail
cd "$(dirname "$0")"

ENV="${1:-fnk0104b}"   # fnk0104b = 2.8", fnk0104s = 4.0"
TAG="${2:-cyds3-web-installer}"
MERGED=../firmware/.pio/build/$ENV/firmware-merged.bin

if [[ ! -f "$MERGED" ]]; then
  echo "error: $MERGED not found — build the firmware first (cd ../firmware && pio run -e $ENV)" >&2
  exit 1
fi

# version comes from the firmware build flags
VERSION=$(sed -nE 's/.*-DFW_VERSION=\\"([^"\\]+)\\".*/\1/p' ../firmware/platformio.ini | head -1)
[[ -n "$VERSION" ]] || { echo "error: could not read FW_VERSION from platformio.ini" >&2; exit 1; }

mkdir -p stage
cp "$MERGED" stage/firmware-merged.bin
cat > stage/manifest.json <<EOF
{
  "name": "CYD-S3 Stereo",
  "version": "$VERSION",
  "new_install_prompt_erase": true,
  "builds": [
    {
      "chipFamily": "ESP32-S3",
      "parts": [{ "path": "firmware/firmware-merged.bin", "offset": 0 }]
    }
  ]
}
EOF

echo "Baking firmware $VERSION into $TAG..."
docker build -t "$TAG" .
echo
echo "Run it:   docker run -d --rm -p 8090:80 $TAG"
echo "Then open http://localhost:8090 in Chrome/Edge on the machine the board is plugged into."
