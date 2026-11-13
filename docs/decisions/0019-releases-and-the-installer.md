# 0019: Release from a version tag, with a catalog in the installer

- Status: accepted; the separate catalog file is replaced by [0023](0023-no-catalog-release-asset.md)
- Date: 2026-11-12

## Context

The repository is public. Users need a download that runs without building from source, and the
app is no use without a catalog. The catalog is built from QuestPorts' live data and QGPUI's
recipes, so it changes more often than the app.

## Decision

- GitHub Actions builds and tests every pull request and every push to `master`
  (`.github/workflows/ci.yml`). The same job builds the installer and keeps it as a workflow
  artifact for a week, so that packaging breaks show up before a release.
- Pushing a tag `v<version>` publishes a GitHub release (`.github/workflows/release.yml`). The tag
  must match the version in `CMakeLists.txt`'s `project()` call; the workflow fails otherwise. That
  version is the only place the version is written. The app reports it with `--version`.
- The installer is made with Inno Setup from the folder that `cmake --install` fills: the app, Qt,
  the MSVC runtime as DLLs beside it (no separate redistributable installer), and adb in
  `platform-tools/` from the runner's Android SDK. It installs for the current user by default and
  needs no administrator rights.
- The release workflow builds a catalog from QuestPorts at release time. The installer puts it next
  to `qgpui.exe`, and the release also offers it as a separate file.
- The app looks for the catalog in this order: the command line, the file opened last,
  `qgpui-catalog.json` in its data folder, the one next to `qgpui.exe`, and in a development build
  the repository's `local/`. A catalog the user put in the data folder wins over the installed one.

## Consequences

- A new catalog reaches users only with a new release, or when they build one and open it.
- Opening the installed catalog once makes it the file opened last, and an upgrade replaces it in
  place, so it stays current across upgrades.
- The release build depends on QuestPorts' Supabase and GitHub being reachable; if either fails,
  the release fails and can be rerun.
- Releasing means raising the version in `CMakeLists.txt`, committing, and pushing the matching
  tag.
