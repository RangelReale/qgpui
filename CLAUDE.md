# qgp

Tools for Meta Quest headsets over adb: finding headsets, managing connections, installing APKs and
copying files. C++20, Qt 6, CMake. The main app, `apps/qgp`, is at an early stage. The rest is the `qadb`
library, a catalog reader and two developer tools.

## Worktrees

Agents work in a git worktree in this repository, never in the main checkout. Call `EnterWorktree`
before the first edit; it creates the feature branch and its checkout under `.claude/worktrees/`
(ignored by git). Leave the worktree with `ExitWorktree` when the work is merged or abandoned.

A new worktree has no `build/` and no `local/`. Configure it with `cmake --preset windows` before
building. To run qgp against the catalog, copy `local/qgp-catalog.json` from the main checkout, or
copy `local/questports-payload.json` and run `task catalog`, or run `task payload` and `task catalog`.

## Layout

- `libs/<name>/`: one folder per library, with `include/<name>/` (public headers), `src/`
  (sources and private headers) and `tests/`. Each library defines a `qgp::<name>` alias target.
- `libs/qadb/`: a Qt client for the adb server. See `libs/qadb/README.md`.
- `tools/<name>/`: developer tools, not shipped.
- `tools/adbmanager/`: a Qt Widgets window with one button per qadb feature and a log. Use it to
  try library changes against a real headset.
- `tools/questports-payload/`: a Node tool, outside the CMake build, that joins the QuestPorts game
  list (Supabase) with each game's install requirements (TypeScript in the QuestPorts repository)
  into one JSON file. `npm install`, then `npm run build-payload` and `npm test`. See its README
  for the payload format. `src/recipes.mjs` checks the install recipes, and `npm run
  build-catalog` (`task catalog`) joins the payload with them into `local/qgp-catalog.json`.
- `data/recipes/<slug>.json`: hand-written install recipes, one per game, following
  `data/recipe.schema.json`. They hold what the payload lacks: the APK source, the user's files and
  the steps. See `docs/install-recipes.md`.
- `local/`: ignored by git. Holds uncommitted working files, such as the generated payload and
  current keys or passwords. Never commit its contents.
- `libs/questports/`: reads the catalog that `tools/questports-payload` writes (`Catalog`, `Game`,
  `Drift`): each game with its recipe and drift. Needs only Qt Core.
- `apps/<name>/`: shipped applications.
- `apps/qgp/`: the main app, Qt Widgets. The main window shows the catalog's games in a `QListView`
  in icon mode: the cover, with the title below. A toolbar's Order box picks a
  `questports::GameOrder`; `GameSortModel` applies it and `QSettings` remembers it. `CoverCache` downloads covers through a
  `QNetworkDiskCache` and fits them into one size. `HeadsetConnection` starts the adb server
  if needed, tracks devices and remembers the current headset (`QSettings`); the toolbar's
  Headset button and the status bar's red/green indicator open `HeadsetDialog`, which lists
  headsets, searches over mDNS, pairs or connects over Wi-Fi, and switches a USB headset to Wi-Fi. The icons in `apps/qgp/icons/` are temporary;
  `make-icon.ps1` draws them (Windows PowerShell). `qgp.rc` gives the .exe its icon.
- `cmake/`: shared CMake functions (`qgp_add_qt_test`).
- `docs/`: research notes, such as where QuestPorts keeps its data and which install processes
  the games need (`install-processes.md`).

## Build and test

- Qt is a prebuilt download in aqtinstall's layout, in the folder named by the `QT_ROOT`
  environment variable (it may be shared between projects), or else `.qt/`. The
  presets set `QGP_QT_SUBDIR` (for example `6.8.3/msvc2022_64`) and `CMakeLists.txt` adds the
  root. CMake caches the Qt it found: configure with `--fresh` after changing `QT_ROOT`.
- Presets: `windows` (Visual Studio 2022; use it locally), `windows-ninja` (needs a developer
  prompt), `macos`.
- `cmake --preset windows`, then `cmake --build --preset windows-debug` and
  `ctest --preset windows-debug`. Use `windows` instead of `windows-debug` for Release.
- `Taskfile.yml` (go-task) wraps these: `task build`, `test`, `run`, `adbmanager`, `clean`,
  `distclean`, `payload`, `payload:test`. It picks the preset from the OS; `CONFIG=Release`
  switches the configuration.
- Run tests through CTest, not the executables directly: CTest puts Qt's DLLs on `PATH` and sets
  `QT_QPA_PLATFORM=offscreen`. Tests log in TAP (`-o -,tap`) because Qt Test's plain-text logger
  prints nothing to a pipe on Windows.
- `tst_qadb_client` runs against `FakeAdbServer`, an in-process fake of the adb server and its
  devices. Extend the fake when the client learns a new request.
- `tst_qadb_live` runs against the real adb server and is skipped by default. Set `QADB_LIVE=1`
  for server requests, `QADB_LIVE_SERIAL=<serial>` for read-only device requests, and
  `QADB_LIVE_WRITE=1` to also push and pull a file in `/data/local/tmp`.
- adb may not be on `PATH`. `qadb::AdbServer::findAdb()` also looks in the Android SDK, at
  `%LOCALAPPDATA%\Android\Sdk\platform-tools\adb.exe`.
- `CLAUDE.local.md` (ignored by git), when present, records where this machine keeps Qt, adb and
  other local checkouts.
- The qgp build copies adb (`adb.exe`, `AdbWinApi.dll`, `AdbWinUsbApi.dll`, `NOTICE.txt` on
  Windows) from `QGP_PLATFORM_TOOLS` (default: the Android SDK's platform-tools) into
  `platform-tools/` next to `qgp.exe`, so users need no Android SDK. `findAdb()` looks there
  right after the `ADB` environment variable. CMake warns and ships no adb when the files are
  missing.
- Builds use `/W4` (MSVC) or `-Wall -Wextra -Wpedantic`. Keep them free of warnings.
- moc reads `//` inside a raw string literal as a comment and then misses the class, which shows
  up as unresolved `metaObject` symbols at link time. In test JSON, write URLs as `https:\/\/`.

## qadb design notes

- The library speaks the adb server's TCP protocol (port 5037) and never runs `adb` except in
  `AdbServer::start()`. Protocol references: `docs/dev/services.md`, `sync.md` and
  `shell_protocol.h` in AOSP `platform/packages/modules/adb`.
- Every request returns a `Reply` (like `QNetworkReply`). Requests start on the next event-loop
  iteration, so callers can connect to `finished()` after the call. Replies are children of the
  `Client` and may be deleted from any of their own signals.
- `detail::Channel` wraps one `QTcpSocket` with callback-style reads (`readExact`,
  `readStatus`, `readHexString`, `readUntilClosed`). A request is a chain of these callbacks.
  Code that emits a reply signal must not touch the channel afterwards, because the handler may
  have deleted the reply and its channel.
- `detail::ReplyAccess` is the library's only write access to replies.
- The real server (version 41) does not always follow the documented answer format. A failed
  `host:pair` answers `OKAY` and then `FAIL` with a message. Check new requests against the
  real server with `tst_qadb_live`, and copy what it does into `FakeAdbServer`.
- Name free functions on library types so that they do not clash with Qt Test's `toString`
  lookup (hence `deviceStateName`, not `toString`).
