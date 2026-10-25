# CI/CD

Flow: GitLab (source of truth) -> GitHub (builds) -> GitLab (releases + Pages).

## Pipelines

- `github-sync` pushes the commit/tag to the GitHub mirror and watches the
  Actions run so a failed GitHub build fails the GitLab pipeline.
- `github-dispatch` triggers a GitHub build on demand (web pipelines).
- `github-mr-build` builds merge requests.
- `auto-release` runs `cog bump --auto` on main and pushes the tag.
- `github-release-sync` downloads the GitHub release and mirrors it as a
  GitLab release.
- `pages` deploys the landing page, the web build under `/app/`, and the
  signed Flatpak repo under `/flatpak/`.

## GitLab CI/CD variables

| Variable | Purpose |
| --- | --- |
| `GITHUB_SSH_PRIVATE_KEY` | SSH key with push access to the GitHub mirror |
| `GITLAB_TOKEN` | PAT (api + write_repository) for the release bump push |
| `GITLAB_RELEASE_SSH_KEY` | Deploy key used to move the release tag |
| `FLATPAK_GPG_KEY` | Base64 armored private key for Flatpak repo signing |
| `GH_TOKEN` | GitHub token for `gh` calls inside GitLab jobs |

## GitHub secrets

| Secret | Purpose |
| --- | --- |
| `GITLAB_TRIGGER_TOKEN` | GitLab pipeline trigger token for release sync |
| `GITLAB_PROJECT_ID` | GitLab project id |
| `FLATPAK_GPG_KEY` | Base64 armored private key for Flatpak repo signing |
