# Changelog fragments

A pull request that changes what people who use QGPUI see adds one file here instead of editing [CHANGELOG.md](../CHANGELOG.md). Pull requests then never change the same lines, so they do not conflict. See [decision 0027](../docs/decisions/0027-changelog-fragments.md).

Name the file after the branch, such as `worktree-uninstall.md`. It holds one or more headings of [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), each with its entries:

```markdown
### Fixed

- Details opens the installation guide at the top. Before, it opened scrolled to the end.
```

The headings are `### Added`, `### Changed`, `### Deprecated`, `### Removed`, `### Fixed` and `### Security`. An entry is a line that starts with `- `, with any further lines indented under it. Write it for people who use QGPUI, not for developers. A change that users do not see, such as a test, a refactor or a CI change, needs no fragment.

CI checks every fragment. To check them locally, from the repository's root:

```sh
python3 tools/release-changelog.py check
```

A release runs `python3 tools/release-changelog.py release X.Y.Z`, which moves the fragments into a section for the new version of `CHANGELOG.md` and deletes them. It does not change the version in `CMakeLists.txt`.
