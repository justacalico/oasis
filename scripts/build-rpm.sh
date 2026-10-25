#!/usr/bin/env bash
# Package a built Flutter Linux bundle as an RPM.
# Usage: build-rpm.sh <bundle-dir> <version> <rpm-arch> <output-file> [release]
set -euo pipefail

BUNDLE_DIR="${1:?usage: build-rpm.sh <bundle-dir> <version> <rpm-arch> <output-file> [release]}"
VERSION="${2:?}"
ARCH="${3:?}"
OUT="${4:?}"
RELEASE="${5:-1}"

TOP=$(mktemp -d)
trap 'rm -rf "$TOP"' EXIT
mkdir -p "$TOP/BUILD" "$TOP/RPMS" "$TOP/SOURCES" "$TOP/SPECS" "$TOP/SRPMS"
mkdir -p "$TOP/root/opt/oasis" "$TOP/root/usr/bin" \
  "$TOP/root/usr/share/applications" "$TOP/root/usr/share/icons/hicolor/512x512/apps"
cp -r "$BUNDLE_DIR/." "$TOP/root/opt/oasis/"
ln -sf /opt/oasis/oasis "$TOP/root/usr/bin/oasis"
install -Dm644 packaging/linux/gitlab.httpanimations.oasis.desktop \
  "$TOP/root/usr/share/applications/gitlab.httpanimations.oasis.desktop"
install -Dm644 packaging/linux/oasis.png \
  "$TOP/root/usr/share/icons/hicolor/512x512/apps/gitlab.httpanimations.oasis.png"

cat > "$TOP/SPECS/oasis.spec" <<SPEC
Name: oasis
Version: $VERSION
Release: $RELEASE
Summary: Oasis desktop shell
License: AGPL-3.0
%description
A Wayland desktop environment shell.
%install
mkdir -p %{buildroot}
cp -r "$TOP/root/." %{buildroot}/
%files
/opt/oasis
/usr/bin/oasis
/usr/share/applications/gitlab.httpanimations.oasis.desktop
/usr/share/icons/hicolor/512x512/apps/gitlab.httpanimations.oasis.png
SPEC

rpmbuild --define "_topdir $TOP" -bb "$TOP/SPECS/oasis.spec"
cp "$TOP/RPMS/$ARCH/"*.rpm "$OUT"
