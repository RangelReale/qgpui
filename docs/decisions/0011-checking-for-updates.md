# 0011: Check the headset's games for updates on their own

- Status: accepted
- Date: 2026-11-12

## Context

[Decision 0006](0006-update-detection.md) says how `installer::updateStatus` compares a release with the package on the headset. It needs the newest release, which only GitHub knows. GitHub allows 60 requests an hour without a token, and qgp has 145 games. Most of their APKs come from GitHub, and several games share one repository.

## Decision

- qgp checks for updates without being asked: each time it reads the current headset (when the headset changes, and after an install). It checks only the games on the headset, not the whole catalog.
- It checks the recipe's first APK, whose release the install record keeps. Only GitHub sources are checked; a URL or manual APK has no release to compare.
- `installer::ReleaseLookup` asks GitHub once per repository and keeps the answer for an hour, and a failure for five minutes. The main window and the game dialogs share one lookup, so opening a game after the check asks GitHub nothing. File > Reload forgets the answers.
- The package list has no `versionName`. qgp reads it with `packageInfo` only when the install record cannot decide on its own (no record, no release tag, or a changed `versionCode`).
- A game with an update gets an Update band on its cover and matches the Headset box's Update available choice. Its game dialog says so, and its Install button becomes Update.
- A failed lookup marks nothing in the list. The game dialog shows the error.

## Consequences

- A headset with up to about 50 games from distinct repositories stays within GitHub's limit for an hourly check. Setting `GITHUB_TOKEN` raises the limit.
- A release published within the hour may not show until the answer expires or the user reloads.
- When GitHub cannot be reached, no game shows an update, and nothing says why in the list.
