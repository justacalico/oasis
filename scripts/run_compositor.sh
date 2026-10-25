#!/usr/bin/env bash
# Builds the Flutter bundle + compositor and runs Oasis inside the current
# Wayland session (nested) or on real hardware with --drm.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

"$ROOT/scripts/fetch_flutter_engine.sh"

if [ ! -f "$ROOT/build/linux/x64/debug/bundle/data/flutter_assets/kernel_blob.bin" ]; then
  flutter build linux --debug
fi

cmake -S "$ROOT/compositor" -B "$ROOT/compositor/build" -G Ninja
ninja -C "$ROOT/compositor/build"

exec "$ROOT/compositor/build/oasis-comp" \
  --bundle "$ROOT/build/linux/x64/debug/bundle" "$@"
