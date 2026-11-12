# 0002: Pick the newest release that has a matching asset

- Status: accepted
- Date: 2026-11-12

## Context

Most APKs come from GitHub releases. GitHub's "latest release" is often wrong for qgp:

- Some repositories hold several games (port0r, lindbergh-recomp-ports), so the latest release can belong to another game.
- Some publish only pre-releases, so there is no latest release.
- Some publish PC-only or Pico builds as newer releases.

## Decision

A recipe's `github` source names the repository, an `asset` pattern, an optional `tag` pattern and whether pre-releases count. qgp takes the newest release (by publish date) whose tag matches and that has an asset matching the pattern, and downloads that asset. Releases without a matching asset are skipped.

## Consequences

- Recipes need no pinned tags for repositories whose newest release leaves the game out; four pins were removed when this rule was adopted.
- The rule is implemented twice, in `installer::pickRelease` and in the scratch resolver used to check recipes, and the two must agree.
- Because the newest qualifying release is taken, a release tag different from the one qgp installed means an update ([decision 0006](0006-update-detection.md)).
