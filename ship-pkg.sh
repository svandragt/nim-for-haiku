#!/usr/bin/env bash
# Build a shippable Haiku package (.hpkg) for the pure-Nim app, on the Haiku box.
# Produces build/counter-1.0.0-1-x86_64.hpkg and pulls it back to ./dist/.
# Usage: ./ship-pkg.sh
set -euo pipefail

HOST="${HAIKU_HOST:-haiku}"
REMOTE_DIR="${HAIKU_DIR:-/boot/home/nim-haiku}"
# Derive the package filename from PackageInfo so the version can't drift.
VER="$(awk '/^version/{print $2}' packaging/PackageInfo)"
HPKG="counter-${VER}-x86_64.hpkg"

rsync -az --mkpath --delete --exclude nimcache/ src/ "$HOST:$REMOTE_DIR/src/"
rsync -az --mkpath packaging/ "$HOST:$REMOTE_DIR/packaging/"

ssh "$HOST" REMOTE_DIR="$REMOTE_DIR" HPKG="$HPKG" bash -s <<'REMOTE'
set -e
cd "$REMOTE_DIR"

# 1. Release build of the pure-Nim app.
mkdir -p build
nim cpp -d:release --hints:off -o:build/Counter src/app.nim

# 2. Embed the app signature + version resource into the binary.
rc -o build/counter.rsrc packaging/counter.rdef
xres -o build/Counter build/counter.rsrc
mimeset -f build/Counter

# 3. Assemble the package tree: apps/Counter installs to <prefix>/apps/Counter.
rm -rf build/pkgroot
mkdir -p build/pkgroot/apps
cp build/Counter build/pkgroot/apps/Counter
cp packaging/PackageInfo build/pkgroot/.PackageInfo

# 4. Build the .hpkg (package create packages the current directory).
cd build/pkgroot
rm -f "../$HPKG"
package create "../$HPKG"
cd "$REMOTE_DIR"
echo "=== built ==="
ls -la "build/$HPKG"
REMOTE

mkdir -p dist
rsync -az "$HOST:$REMOTE_DIR/build/$HPKG" "dist/$HPKG"
echo "pulled -> dist/$HPKG"
