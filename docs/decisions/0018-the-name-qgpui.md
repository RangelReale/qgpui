# 0018: The app is named QGPUI

- Status: accepted
- Date: 2026-11-12

## Context

The project was called qgp. The official name of the app is now QGPUI. Records before this one use the old name and are not edited.

## Decision

The app's name, as users see it, is QGPUI, in upper case. Everything with a name in the file system or the code uses qgpui, in lower case: the repository, the executable (`qgpui.exe`), `apps/qgpui`, the icons, the CMake project and its `qgpui::` targets, the `QGPUI_` CMake options and environment variables, and the format names (`qgpui-catalog`, `qgpui-install-recipe`, `qgpui-questports-payload`, `qgpui-install-records`, `qgpui-headset-installs`).

The names of stored data change too:

- QSettings: organization and application `qgpui`, so the settings live under `HKEY_CURRENT_USER\Software\qgpui` on Windows.
- The app's data folder (the catalog and the PC's copy of the install records): `%APPDATA%\qgpui`.
- The default files folder: `Documents\qgpui\files`, with a `qgpui - what goes here.txt` guide in each game folder.
- The install records on each headset: `/sdcard/qgpui/installs.json`.

The app reads no data under the old names. QGPUI is at an early stage with no users besides its developer, so moving old data by hand is cheaper than keeping code that reads both names.

## Consequences

- The research documents and the earlier decisions keep the name qgp, as they were written.
- Existing data is lost to the app unless moved: the registry key `Software\qgp`, the folder `%APPDATA%\qgp`, the files folder if it was the default, and `/sdcard/qgp/` on each headset. Without the headset folder, games installed before the rename show as installed outside QGPUI.
- Old guide files named `qgp - what goes here.txt` are no longer recognised as guides. They would be pushed if a push step takes every file of an input, so delete or rename them.
- The data formats change name, so a catalog built before the rename does not load. Run `task catalog` again.
