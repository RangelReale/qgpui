# qgp

Tools for Meta Quest headsets over adb: finding headsets, managing connections, installing APKs and
copying files. C++20, Qt 6, CMake. The main app does not exist yet; the work so far is the `qadb`
library and a debug tool.

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
  for the payload format.
- `local/`: ignored by git. Holds uncommitted working files, such as the generated payload and
  current keys or passwords. Never commit its contents.
- `apps/<name>/`: shipped applications, when there are any.
- `cmake/`: shared CMake functions (`qgp_add_qt_test`).
- `docs/`: research notes, such as where QuestPorts keeps its data.

## Build and test

- Qt is a prebuilt download in aqtinstall's layout, in the folder named by the `QT_ROOT`
  environment variable (shared between projects; `F:\Qt` on the main PC), or else `.qt/`. The
  presets set `QGP_QT_SUBDIR` (for example `6.8.3/msvc2022_64`) and `CMakeLists.txt` adds the
  root. CMake caches the Qt it found: configure with `--fresh` after changing `QT_ROOT`.
- Presets: `windows` (Visual Studio 2022; use it locally), `windows-ninja` (needs a developer
  prompt), `macos`.
- `cmake --preset windows`, then `cmake --build --preset windows-debug` and
  `ctest --preset windows-debug`. Use `windows` instead of `windows-debug` for Release.
- Run tests through CTest, not the executables directly: CTest puts Qt's DLLs on `PATH` and sets
  `QT_QPA_PLATFORM=offscreen`. Tests log in TAP (`-o -,tap`) because Qt Test's plain-text logger
  prints nothing to a pipe on Windows.
- `tst_qadb_client` runs against `FakeAdbServer`, an in-process fake of the adb server and its
  devices. Extend the fake when the client learns a new request.
- `tst_qadb_live` runs against the real adb server and is skipped by default. Set `QADB_LIVE=1`
  for server requests, `QADB_LIVE_SERIAL=<serial>` for read-only device requests, and
  `QADB_LIVE_WRITE=1` to also push and pull a file in `/data/local/tmp`.
- The adb binary is at `%LOCALAPPDATA%\Android\Sdk\platform-tools\adb.exe` on the main PC; it is
  not on `PATH`. `qadb::AdbServer::findAdb()` finds it there.
- Builds use `/W4` (MSVC) or `-Wall -Wextra -Wpedantic`. Keep them free of warnings.

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
