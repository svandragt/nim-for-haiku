#!/usr/bin/env bash
# Build the todo app on Haiku, run it with --demo (seeds a few rows via the
# runtime add-after-show path), screenshot the desktop, then quit it and pull
# the PNG back. Proves the window and its dynamic todo rows actually render.
# Usage: ./test-todo.sh [local-screenshot-path]
set -euo pipefail

HOST="${HAIKU_HOST:-haiku}"
REMOTE_DIR="${HAIKU_DIR:-/boot/home/nim-haiku}"
LOCAL_SHOT="${1:-todo-shot.png}"

rsync -az --mkpath --delete --exclude nimcache/ src/ "$HOST:$REMOTE_DIR/src/"

ssh "$HOST" REMOTE_DIR="$REMOTE_DIR" bash -s <<'REMOTE'
set -e
cd "$REMOTE_DIR"
mkdir -p bin
nim cpp --hints:off -o:bin/todo src/todo.nim
./bin/todo --demo > todo.log 2>&1 &
APP=$!
sleep 1
screenshot -s -f png "$REMOTE_DIR/todo-shot.png" || true
quit application/x-vnd.nim-todo || kill "$APP" || true
wait "$APP" 2>/dev/null || true
echo "=== todo.log ==="
cat todo.log
REMOTE

scp -q "$HOST:$REMOTE_DIR/todo-shot.png" "$LOCAL_SHOT"
echo "screenshot -> $LOCAL_SHOT"
