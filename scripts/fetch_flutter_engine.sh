#!/usr/bin/env bash
# Downloads the Flutter embedder engine (libflutter_engine.so + icudtl.dat)
# matching the installed Flutter SDK into compositor/engine/.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/compositor/engine"
mkdir -p "$OUT"

FLUTTER="${FLUTTER:-flutter}"
VERSION_JSON="$(dirname "$(readlink -f "$(command -v "$FLUTTER")")")/cache/flutter.version.json"

if [ -n "${OASIS_ENGINE_HASH:-}" ]; then
  HASH="$OASIS_ENGINE_HASH"
elif [ -f "$VERSION_JSON" ]; then
  HASH="$(python3 -c 'import json,sys;print(json.load(open(sys.argv[1]))["engineContentHash"])' "$VERSION_JSON")"
else
  echo "cannot determine engine content hash; set OASIS_ENGINE_HASH" >&2
  exit 1
fi

BASE="https://storage.googleapis.com/flutter_infra_release/flutter/$HASH/linux-x64"

if [ ! -f "$OUT/libflutter_engine.so" ]; then
  echo "fetching linux-x64-embedder.zip ($HASH)"
  curl -fSL "$BASE/linux-x64-embedder.zip" -o "$OUT/embedder.zip"
  unzip -o "$OUT/embedder.zip" libflutter_engine.so -d "$OUT"
  rm "$OUT/embedder.zip"
fi

if [ ! -f "$OUT/icudtl.dat" ]; then
  echo "fetching artifacts.zip for icudtl.dat"
  curl -fSL "$BASE/artifacts.zip" -o "$OUT/artifacts.zip"
  unzip -o "$OUT/artifacts.zip" icudtl.dat vm_isolate_snapshot.bin isolate_snapshot.bin -d "$OUT"
  rm "$OUT/artifacts.zip"
fi

echo "engine ready: $OUT"
