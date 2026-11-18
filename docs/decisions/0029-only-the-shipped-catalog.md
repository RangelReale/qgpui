# 0029: QGPUI reads only the catalog it shipped with

- Status: accepted
- Date: 2026-11-18

## Context

[Decision 0019](0019-releases-and-the-installer.md) let QGPUI take its catalog from several places,
in this order: the command line, the file opened last, `qgpui-catalog.json` in the app's data
folder, the catalog next to `qgpui.exe`, and in a development build the repository's `local/`.
File > Open Catalog picked another file, and every catalog QGPUI loaded became the file opened last.

After the 1.3.0 release, the installed app kept showing the 145 games of an older catalog. A
development build had loaded `local/qgpui-catalog.json` and saved it as the file opened last. The
installed app shares the same settings, so it reopened that file and never reached the catalog
the installer had just updated. Any user who had opened another catalog, or who had put one in the
data folder, would hit the same problem after an update.

Each release builds its catalog from QuestPorts and ships it inside the installer and the disk
image ([decision 0023](0023-no-catalog-release-asset.md)). Users have no reason to pick another one.

## Decision

QGPUI reads only the catalog it shipped with: `qgpui-catalog.json` next to `qgpui.exe`, or in
`Contents/Resources` of the macOS bundle. A development build (`QGPUI_DEV_CATALOG`) falls back to
the repository's `local/qgpui-catalog.json` when no catalog sits next to the app.

- File > Open Catalog is removed. File > Reload stays and reads the same file again.
- QGPUI no longer takes a catalog on the command line, and no longer remembers one. On start it
  deletes the `catalogPath` setting that older versions saved.
- A `qgpui-catalog.json` in the data folder is ignored.
- A second start of QGPUI only brings the first window to the front
  ([decision 0012](0012-single-instance.md) passed a catalog argument along).

This replaces the catalog lookup order of decision 0019.

## Consequences

- An update always brings its catalog into use.
- Users get new games and recipes only with a new release.
- A developer tries a catalog by writing it to `local/` with `task catalog` and running a
  development build, or by copying it next to a built `qgpui.exe`.
