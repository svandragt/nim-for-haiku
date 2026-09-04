#!/usr/bin/env bash
# Build the Nim+BeAPI GUI app on Haiku, run it, screenshot the desktop, quit it,
# and pull the screenshot back. Proves the window actually renders.
# Usage: ./test-gui.sh [local-screenshot-path]
set -euo pipefail

HOST="${HAIKU_HOST:-haiku}"
REMOTE_DIR="${HAIKU_DIR:-/boot/home/nim-haiku}"
LOCAL_SHOT="${1:-gui-shot.png}"

rsync -az --mkpath --delete --exclude nimcache/ src/ "$HOST:$REMOTE_DIR/src/"

ssh "$HOST" REMOTE_DIR="$REMOTE_DIR" bash -s <<'REMOTE'
set -e
cd "$REMOTE_DIR"
mkdir -p bin
nim cpp --hints:off -o:bin/gui src/gui.nim
./bin/gui > gui.log 2>&1 &
APP=$!
sleep 1                 # window is up; app self-quits at 3s
screenshot -s -f png "$REMOTE_DIR/gui-shot.png" || true
wait "$APP"             # clean exit, so the BeAPI round-trip output lands
echo "=== gui.log ==="
cat gui.log
REMOTE

scp -q "$HOST:$REMOTE_DIR/gui-shot.png" "$LOCAL_SHOT"
echo "screenshot -> $LOCAL_SHOT"
