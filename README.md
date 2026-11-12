# qgp

Tools for Meta Quest headsets over adb: finding headsets, managing connections, installing APKs and
copying files.

- [`apps/qgp`](apps/qgp): the main application. For now it shows the QuestPorts games as a grid of
  covers.
- [`libs/gamestores`](libs/gamestores): finds the games installed from Steam and GOG on this PC, and
  the GOG games the user owns.
- [`libs/qadb`](libs/qadb/README.md): a Qt 6 client library for the adb server.
- [`libs/questports`](libs/questports): reads the qgp catalog, which holds the QuestPorts games and
  their install recipes.
- [`tools/adbmanager`](tools/adbmanager): a debug window for trying qadb on a real headset.
- [`tools/questports-payload`](tools/questports-payload/README.md): builds one JSON file with the
  QuestPorts game list and each game's install requirements (Node).
- [`docs`](docs/README.md): research, architecture and decision records.

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

The app is `build/windows/bin/Debug/qgp.exe` and the debug tool is
`build/windows/bin/Debug/adbmanager.exe`.

The app reads the catalog that `tools/questports-payload` builds with `build-catalog` (`task
catalog`). It takes the file from its command line, or else the file opened last, or else
`qgp-catalog.json` in its data folder (`%APPDATA%\qgp` on Windows). Development builds also look in `local/` in the repository, where the
tool writes it by default.
