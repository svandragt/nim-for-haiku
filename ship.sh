#!/usr/bin/env bash
# Ship src/ to the Haiku box and build+run a target there.
# Usage: ./ship.sh <target.nim> [nim-backend]   e.g. ./ship.sh hello       ./ship.sh gui cpp
set -euo pipefail

HOST="${HAIKU_HOST:-haiku}"
REMOTE_DIR="${HAIKU_DIR:-/boot/home/nim-haiku}"
TARGET="${1:?usage: ship.sh <target.nim without extension> [c|cpp]}"
BACKEND="${2:-c}"   # c for CLI, cpp for BeAPI/libbe code

rsync -az --mkpath --delete --exclude nimcache/ src/ "$HOST:$REMOTE_DIR/src/"
ssh "$HOST" "cd $REMOTE_DIR && nim $BACKEND -r --hints:off src/$TARGET.nim"
