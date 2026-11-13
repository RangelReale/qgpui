# 0027: Changelog fragments and release notes

- Status: accepted
- Date: 2026-11-13

## Context

QGPUI had no changelog. Its releases took their notes from GitHub's list of merged pull requests,
which is written for developers.

A changelog in [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) format keeps its unreleased
changes in one list under `## [Unreleased]`. Several sessions work on QGPUI in parallel, each in its
own pull request. If each pull request added its entry to that list, every merge would make the other
open pull requests conflict on the same lines, and each would need `master` merged in and CI run
again.

Mirante, another project of the same author, had this problem and solved it with changelog
fragments (decision 0154 in `mirante-core`). QGPUI takes the same approach.

## Decision

- `CHANGELOG.md` follows Keep a Changelog, with a section per released version.
- **A pull request does not edit `CHANGELOG.md`.** It adds its entries as a file of its own, a
  fragment, in `changelog.d/`, named after its branch. A fragment holds headings such as
  `### Fixed`, each with its entries. `changelog.d/README.md` says how to write one. A change that
  users do not see needs no fragment.
- **A release moves the fragments into `CHANGELOG.md`.** `tools/release-changelog.py release X.Y.Z`
  makes the section `## [X.Y.Z]` from the entries under Unreleased and the fragments, grouped by
  heading, adds its comparison link and deletes the fragments. The pull request that raises the
  version in `project()` runs it.
- **The release notes start with the changelog.** `release.yml` takes the version's section of
  `CHANGELOG.md` with `tools/release-changelog.py notes X.Y.Z`, before it builds anything, and
  fails without one. Links to files of the repository point at the tag, since a release page does
  not resolve relative links. GitHub's generated list of merged pull requests follows the section.
- **CI checks the fragments.** The `changelog` job in `ci.yml` runs
  `tools/release-changelog.py check` and fails on a fragment that is not well formed.

## Alternatives

- **A union merge driver** (`CHANGELOG.md merge=union` in `.gitattributes`). GitHub ignores merge
  drivers, so its pages still show a conflict. Two pull requests that each add the same heading
  give that heading twice.
- **Notes written at release time** from the pull requests. The entries would be written long
  after the change, often without its context.
- **An existing tool,** such as towncrier or changesets. Each brings a dependency and its own
  format, for a task that a short script does. The script is adapted from `mirante-core`'s.

## Consequences

- Pull requests no longer conflict on the changelog. They can still conflict on code.
- An entry is not in `CHANGELOG.md` until the release. What is unreleased is the entries under
  Unreleased and the files in `changelog.d/`.
- A changelog section is also the public text of a release, so it is written for the people who
  read it. A tag pushed without running `release X.Y.Z` first fails the release.
- An edit to a section after its release does not change the published notes. `gh release edit`
  does that.
