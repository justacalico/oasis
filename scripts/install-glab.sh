#!/usr/bin/env bash
set -euo pipefail

# Load per-project CI config (APP_NAME, APP_ID, GITHUB_REPO, GITLAB_PROJECT)
_CI_DIR="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)"
[ -f "$_CI_DIR/ci.env" ] && . "$_CI_DIR/ci.env"

if command -v glab >/dev/null 2>&1; then
  glab --version
  exit 0
fi

version="${GLAB_VERSION:-1.113.0}"
arch="${CI_RUNNER_ARCH:-amd64}"
case "$arch" in
  arm|armv7l) arch="armv6" ;;
  aarch64|arm64) arch="arm64" ;;
  386|i386) arch="386" ;;
  x86_64|amd64) arch="amd64" ;;
esac

url="https://gitlab.com/gitlab-org/cli/-/releases/v${version}/downloads/glab_${version}_linux_${arch}.tar.gz"
mkdir -p /usr/local/bin
curl -fsSL "$url" -o /tmp/glab.tar.gz
tar -xzf /tmp/glab.tar.gz -C /tmp --strip-components=1
cp /tmp/glab /usr/local/bin/glab
chmod +x /usr/local/bin/glab
glab --version
