#!/usr/bin/env bash
set -euo pipefail

_CI_DIR="$(cd "$(dirname "$0")" && pwd)"
[ -f "$_CI_DIR/ci.env" ] && . "$_CI_DIR/ci.env"

export PATH="$HOME/.local/bin:$PATH"

RELEASE_TAG="${RELEASE_TAG:-}"
GITHUB_RUN_ID="${GITHUB_RUN_ID:-}"
PROJECT_DIR="${CI_PROJECT_DIR:-$PWD}"
GITHUB_REPO="${GITHUB_REPO:?set GITHUB_REPO in scripts/ci.env}"

if [ -z "$RELEASE_TAG" ]; then
  RELEASE_TAG=$(gh release view -R "$GITHUB_REPO" --json tagName -q .tagName)
fi
echo "Syncing GitHub release $RELEASE_TAG to GitLab"

RELEASE_COMMIT=$(gh api "repos/$GITHUB_REPO/commits?sha=$RELEASE_TAG&per_page=1" -q '.[0].sha' 2>/dev/null || true)
[ -n "$RELEASE_COMMIT" ] || RELEASE_COMMIT=$(git rev-parse "$RELEASE_TAG" 2>/dev/null || true)

mkdir -p release-assets
gh release download "$RELEASE_TAG" -R "$GITHUB_REPO" --dir release-assets --clobber

if [ -n "$GITHUB_RUN_ID" ]; then
  gh run view "$GITHUB_RUN_ID" -R "$GITHUB_REPO" --log > github-logs.txt 2>/dev/null || true
fi

cd "$PROJECT_DIR"
[ -f SHA256SUMS.txt ] && cp SHA256SUMS.txt release-assets/ || true
ls -la release-assets/

glab release delete "$RELEASE_TAG" -R "$CI_PROJECT_PATH" -y 2>/dev/null || true
PKG_ID=$(glab api "projects/$CI_PROJECT_ID/packages?package_name=release-assets&package_version=$RELEASE_TAG" 2>/dev/null | jq -r '.[0].id // empty' 2>/dev/null || true)
if [ -n "$PKG_ID" ] && [ "$PKG_ID" != "null" ]; then
  glab api --method DELETE "projects/$CI_PROJECT_ID/packages/$PKG_ID" 2>/dev/null || true
fi

if [ -n "${GITLAB_RELEASE_SSH_KEY:-}" ]; then
  git remote add gitlab-ssh "git@gitlab.com:${CI_PROJECT_PATH}.git" 2>/dev/null || true
  git tag -f "$RELEASE_TAG" "$RELEASE_COMMIT" 2>/dev/null || true
  git push -f gitlab-ssh "$RELEASE_TAG" 2>/dev/null || true
fi

glab release create "$RELEASE_TAG" \
  --name "${APP_NAME:-$RELEASE_TAG} $RELEASE_TAG" \
  --notes "Mirrored from the GitHub release." \
  --ref "$RELEASE_COMMIT" \
  --use-package-registry \
  "$PROJECT_DIR/release-assets"/*
