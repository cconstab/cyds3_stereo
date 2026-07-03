#!/usr/bin/env bash
# Build a release and publish it to the update server.
# Usage: ./scripts/release.sh <version> [server-url] [notes]
#   UPLOAD_TOKEN env var is sent as the auth token.
# Example: UPLOAD_TOKEN=secret ./scripts/release.sh 0.2.0 http://192.168.1.50:8080 "fix VU scaling"
set -euo pipefail
cd "$(dirname "$0")/.."

VERSION="${1:?usage: release.sh <version> [server-url] [notes] [env]}"
SERVER="${2:-}"
NOTES="${3:-}"
ENV="${4:-fnk0104b}"   # fnk0104b = 2.8", fnk0104s = 4.0"
PIO="${PIO:-pio}"

# device channel on the update server comes from the env's FW_DEVICE flag
DEVICE=$(awk "/^\[env:$ENV\]/,/^\[/" platformio.ini | sed -nE 's/.*-DFW_DEVICE=\\"([^"\\]+)\\".*/\1/p' | head -1)
[[ -n "$DEVICE" ]] || { echo "error: could not read FW_DEVICE for env $ENV" >&2; exit 1; }

if ! [[ "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+(\.[0-9]+)?$ ]]; then
  echo "version must look like 1.2.3" >&2
  exit 1
fi

# stamp the version into platformio.ini
sed -i.bak -E "s/-DFW_VERSION=\\\\\"[^\"]*\\\\\"/-DFW_VERSION=\\\\\"$VERSION\\\\\"/" platformio.ini
rm -f platformio.ini.bak
echo "Building firmware $VERSION for $ENV (device channel: $DEVICE)..."
"$PIO" run -e "$ENV"

BIN=.pio/build/$ENV/firmware.bin
MERGED=.pio/build/$ENV/firmware-merged.bin
echo "app image:    $(shasum -a 256 "$BIN")"
echo "merged image: $(shasum -a 256 "$MERGED")"

if [[ -n "$SERVER" ]]; then
  echo "Publishing to $SERVER..."
  curl -sf -X POST "$SERVER/api/upload" \
    ${UPLOAD_TOKEN:+-H "Authorization: Bearer $UPLOAD_TOKEN"} \
    -F "device=$DEVICE" \
    -F "version=$VERSION" \
    -F "notes=$NOTES" \
    -F "firmware=@$BIN" \
    -F "merged=@$MERGED"
  echo
  echo "Published $VERSION — devices will pick it up on their next check."
else
  echo "No server URL given — build only. Upload $BIN (+ merged) on the dashboard."
fi
