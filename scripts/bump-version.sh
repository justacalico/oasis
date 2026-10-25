#!/usr/bin/env bash
set -euo pipefail

VERSION="$1"
[ -n "$VERSION" ] || { echo "version required" >&2; exit 1; }

sed -i "s/^version: .*/version: ${VERSION}+$(date +%s)/" pubspec.yaml
grep -q "^version: ${VERSION}+" pubspec.yaml
