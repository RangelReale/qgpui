# Developing QGPUI

This page is for people who build QGPUI from source or work on its code. The [README](README.md)
describes the app for its users.

## Repository layout

- [`apps/qgpui`](apps/qgpui): the main application, Qt Widgets.
- [`libs/gamestores`](libs/gamestores): finds the games installed from Steam and GOG on this PC, and
  the GOG games the user owns.
- [`libs/installer`](libs/installer): runs install recipes on a headset, detects updates and keeps
  install records.
- [`libs/qadb`](libs/qadb/README.md): a Qt 6 client library for the adb server.
- [`libs/questports`](libs/questports): reads the QGPUI catalog, which holds the QuestPorts games and
  their install recipes.
- [`tools/adbmanager`](tools/adbmanager): a debug window for trying qadb on a real headset.
- [`tools/questports-payload`](tools/questports-payload/README.md): builds one JSON file with the
  QuestPorts game list and each game's install requirements, and joins it with the recipes into
  the catalog (Node).
- [`data/recipes`](data/recipes): the hand-written install recipe of each game.
- [`docs`](docs/README.md): research, architecture and decision records.
  [The architecture overview](docs/architecture/overview.md) shows how the parts fit together.

## Build

Requirements: CMake 3.25, Qt 6.8 (Core, Network, Sql, Widgets, Test), a C++20 compiler. Set
`QT_ROOT` to the folder that holds Qt in aqtinstall's layout (for example `QT_ROOT=C:\Qt` with Qt
in `C:\Qt\6.8.3\msvc2022_64`).

```bash
cmake --preset windows
cmake --build --preset windows-debug
ctest --preset windows-debug
```

With [go-task](https://taskfile.dev) installed, `task build`, `task test`, `task run`, `task clean`
and `task distclean` do the same for the current OS (`task --list` shows all tasks). Add
`CONFIG=Release` for a Release build.

The app is `build/windows/bin/Debug/qgpui.exe` and the debug tool is
`build/windows/bin/Debug/adbmanager.exe`.

The build copies adb from the Android SDK's platform-tools (or the folder named by
`QGPUI_PLATFORM_TOOLS`) into `platform-tools/` next to `qgpui.exe`, so users need no Android SDK.
CMake warns and ships no adb when the files are missing.

## The catalog

The app reads the catalog that `tools/questports-payload` builds. It needs Node 20 or later.

```bash
task payload
task catalog
```

`task payload` downloads the QuestPorts game list and install requirements into
`local/questports-payload.json`. `task catalog` joins them with `data/recipes/` into
`local/qgpui-catalog.json`.

The app takes the catalog from its command line, or else the file opened last, or else
`qgpui-catalog.json` in its data folder (`%APPDATA%\qgpui` on Windows). Development builds also
look in `local/` in the repository, where the tool writes it by default.

## Tests

Run tests through CTest, not the executables directly: CTest puts Qt's DLLs on `PATH`. Some tests
run against a real adb server, headset or game library, and are skipped unless an environment
variable turns them on:

- `tst_qadb_live`: `QADB_LIVE=1` for server requests, `QADB_LIVE_SERIAL=<serial>` for read-only
  device requests, and `QADB_LIVE_WRITE=1` to also push and pull a file in `/data/local/tmp`.
- `tst_gamestores_live`: `QGPUI_STORES_LIVE=1` reads this PC's Steam and GOG libraries and matches
  every recipe against them. Read its output with `ctest -V`.
