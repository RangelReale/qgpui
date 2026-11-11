# qgp

Tools for Meta Quest headsets over adb: finding headsets, managing connections, installing APKs and
copying files.

- [`libs/qadb`](libs/qadb/README.md): a Qt 6 client library for the adb server.
- [`tools/adbmanager`](tools/adbmanager): a debug window for trying qadb on a real headset.
- [`docs`](docs): research notes.

## Build

Requirements: CMake 3.25, Qt 6.8 (Core, Network, Widgets, Test), a C++20 compiler. Set `QT_ROOT` to
the folder that holds Qt in aqtinstall's layout (for example `QT_ROOT=F:\Qt` with Qt in
`F:\Qt\6.8.3\msvc2022_64`).

```bash
cmake --preset windows
cmake --build --preset windows-debug
ctest --preset windows-debug
```

The debug tool is `build/windows/bin/Debug/adbmanager.exe`.
