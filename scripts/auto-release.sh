#!/bin/bash
set -euo pipefail

_CI_DIR="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)"
[ -f "$_CI_DIR/ci.env" ] && . "$_CI_DIR/ci.env"

git fetch origin main "+refs/tags/*:refs/tags/*"
git checkout -B main origin/main
git clean -fd

cog bump --auto --hook-profile ci

TAG=$(git tag --points-at HEAD | head -n1)
[ -n "$TAG" ] || { echo "cog bump did not create a tag" >&2; exit 1; }

push_bump() { git push -o ci.skip origin HEAD:main; }
if ! push_bump; then
  echo "Main moved while bumping, rebasing onto latest origin/main"
  git fetch origin main "+refs/tags/*:refs/tags/*"
  git rebase origin/main
  git tag -f "$TAG"
  push_bump
fi

git push origin "$TAG"
echo "Released $TAG"
