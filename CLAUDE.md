# QGPUI

Tools for Meta Quest headsets over adb: finding headsets, managing connections, installing APKs and
copying files. C++20, Qt 6, CMake. The main app, `apps/qgpui`, is at an early stage. The rest is the `qadb`
library, a catalog reader and two developer tools.

## Worktrees

Agents work in a git worktree in this repository, never in the main checkout. Call `EnterWorktree`
before the first edit; it creates the feature branch and its checkout under `.claude/worktrees/`
(ignored by git). Leave the worktree with `ExitWorktree` when the work is merged or abandoned.

`master` is protected on GitHub: it changes only through a pull request whose CI checks pass, and
the rule has no bypass. Do not merge into `master` locally, because the push is rejected. Push the
branch, open a pull request, and merge it with `gh pr merge <number> --merge` (or auto-merge) once
CI passes. GitHub deletes the remote branch after the merge. Do not pass `--delete-branch` in a
worktree: it tries to check out `master`, which the main checkout holds.

A new worktree has no `build/` and no `local/`. Configure it with `cmake --preset windows` before
building. To run QGPUI against the catalog, copy `local/qgpui-catalog.json` from the main checkout, or
copy `local/questports-payload.json` and run `task catalog`, or run `task payload` and `task catalog`.

## Layout

- `libs/<name>/`: one folder per library, with `include/<name>/` (public headers), `src/`
  (sources and private headers) and `tests/`. Each library defines a `qgpui::<name>` alias target.
- `libs/qadb/`: a Qt client for the adb server. See `libs/qadb/README.md`.
- `tools/<name>/`: developer tools, not shipped.
- `tools/adbmanager/`: a Qt Widgets window with one button per qadb feature and a log. Use it to
  try library changes against a real headset.
- `tools/questports-payload/`: a Node tool, outside the CMake build, that joins the QuestPorts game
  list (Supabase) with each game's install requirements (TypeScript in the QuestPorts repository)
  into one JSON file. `npm install`, then `npm run build-payload` and `npm test`. See its README
  for the payload format. `src/recipes.mjs` checks the install recipes, and `npm run
  build-catalog` (`task catalog`) joins the payload with them into `local/qgpui-catalog.json`.
- `data/recipes/<slug>.json`: hand-written install recipes, one per game, following
  `data/recipe.schema.json`. They hold what the payload lacks: the APK source, the user's files and
  the steps. See `docs/architecture/install-recipes.md`.
- `local/`: ignored by git. Holds uncommitted working files, such as the generated payload and
  current keys or passwords. Never commit its contents.
- `libs/questports/`: reads the catalog that `tools/questports-payload` writes (`Catalog`, `Game`,
  `Drift`): each game with its recipe, its drift and the fields the site's filters use. Needs only Qt Core.
- `libs/gamestores/`: finds the PC installs that recipes ask for. `SteamLibrary` reads every library
  in `libraryfolders.vdf` (KeyValues, parsed by `parseVdf`) and its `appmanifest_*.acf` files.
  `GogLibrary` merges the registry and GOG Galaxy's SQLite database (installs and ownership; it
  opens a temporary copy). `GameFinder::find` takes a recipe's `pc-install` input and matches by
  ID, then by folder name, then by the input's `detect` files in every installed game. `FilesFolder` looks for any input's files in the
  user's files folder: `<slug>/`, `<slug>/<input id>/` where inputs overlap, and shared files
  anywhere (decision 0009). Qt Core and Qt Sql (SQLite driver).
- `libs/installer/`: runs install recipes. `parseRecipe` reads a catalog recipe into `Recipe`;
  `ApkFetcher` resolves a github source (`pickRelease`) and downloads the APK into a cache;
  `planPush` turns a push step and the user's files into single-file copies; `Runner` runs the
  steps on a device through qadb; `planInstall` lists the commands it will run, for the game dialog's Details. Below `Android/data` and `Android/obb` it creates folders with
  `mkdir` and `chmod 2777` before pushing (decision 0003). It checks
  `freeSpaceMB` first, keeps a newer build on the headset, and explains a changed signing key.
  `Updates.h` compares versions and decides an `UpdateStatus`; `ReleaseLookup` finds a source's
  newest release without downloading it, one GitHub request per repository, kept for an hour. Install records live on each
  headset in `/sdcard/qgpui/installs.json`; `InstallRecords` is the PC's cache of them (keyed by the
  headset's `ro.serialno`, then the package), and `HeadsetRecordSync` keeps the two in step.
  `KeepAwake` keeps a Quest's screen on with the proximity-sensor broadcast, leaves an override
  that another app holds alone, and `main()` releases them all on exit (decisions 0013 to 0015).
  Its runner, sync and keep-awake tests use qadb's `FakeAdbServer`.
- `apps/<name>/`: shipped applications.
- `apps/qgpui/`: the main app, Qt Widgets. The main window shows the catalog's games in a `QListView`
  in icon mode: the cover, with the title below. A toolbar's Order box picks a
  `questports::GameOrder`; `GameSortModel` applies it and `QSettings` remembers it. Clear, at the right end of that row, clears every
  filter. Four more boxes filter, and a
  game must match all of them (`GameFilter`): Source (the site's origin platform,
  `resolved.originPlatform`), Store (sold on Steam or GOG, from the recipe's `stores`, or in this
  PC's Steam or GOG library, from `GameFinder` when the catalog loads), Installed (on the current
  headset, installed by QGPUI, installed outside QGPUI, i.e. with no entry in the headset's install
  records, not on it, or with an update) and Self-contained only (an automatic recipe, no manual
  APK, and every required input and each input group free). A second row holds the site's own
  filters: Runs on (`resolved.headsets`), VR style (`resolved.vrStyle`), No PC needed
  (`resolved.noPcInstall`), Type, Developer and Status (from the port row).
  `GameFlags` computes the flags; `MainWindow::findHeadsetGames` reads the headset when it changes
  and after an install, and `checkForUpdate` compares each game on it with its APK's newest
  release (decision 0011); a game on the headset gets an Installed tag on its cover, and one with
  an update an Update tag beside it; every game also gets a tag for its port status, one colour
  each. The filters are not saved: the app always starts with every filter clear, and only the
  order is kept in `QSettings` (`main.cpp` sets the organization name, without which QSettings
  stores nothing on Windows). The game dialog's header shows badges (on the headset,
  update, source, type, port status), the recipe's
  `otherWays` (the port's own installers, with links) highlighted, and one line of
  links: the game's QuestPorts page, and the Steam and GOG pages of each `pc-install` input
  (GOG through its product API). `CoverCache` downloads covers through a
  `QNetworkDiskCache` and fits them into one size. `HeadsetConnection` starts the adb server
  if needed (and `main()` stops it on exit if QGPUI started it, decision 0021), tracks devices and remembers the current headset (`QSettings`) by adb serial and by
  hardware serial, so that it is found again when its wireless debugging port changes. It also holds
  the toolbar's keep-awake button (an eye icon next to Headset), which keeps the current headset's
  screen on until turned off and overrides the install setting. The toolbar's
  Headset button and the status bar's red/green indicator open `HeadsetDialog`, which lists
  headsets, searches over mDNS, pairs or connects over Wi-Fi, and switches a USB headset to Wi-Fi. Opening a game (double-click, Enter,
  Game > Open, context menu) shows `GameDialog`, where every action on a game starts (decision
  0010): its inputs as `InputResolver` finds them (Steam and GOG, then the files folder) with a
  Choose button each and, below each, what it needs in the words of the files folder's guide
  (`FilesFolder::inputGuide`), Custom Files Folder (creates the game's folder and its
  `qgpui - what goes here.txt`), the port's `base_game_store` highlighted just above Install, and Install, which fetches the APK, runs `installer::Runner` with
  a progress bar, Continue for tell steps and a log, then records the install with
  `HeadsetRecordSync` and shows the result in a coloured banner. Details shows two tabs: the port's `installation_guide` (Markdown, drawn by `GuideView` in the site's colours, with a workaround for Qt's importer putting a code block inside a list item) above its `troubleshooting_notes`, as on the port's page, and `planInstall`'s commands. File > Settings (`SettingsDialog`)
  holds the preferences: the files folder and keeping the headset awake during installs. The icons in `apps/qgpui/icons/` are temporary;
  `make-icon.ps1` draws them (Windows PowerShell). On macOS the app uses `icons/macos/` instead, 16 to 1024 pixels on Apple's icon grid, which `icons/macos/make-icons.sh` renders from `qgpui.svg` with `rsvg-convert`. `qgpui.rc` gives the .exe its icon. `SingleInstance`
  keeps one QGPUI per user: a second start passes its catalog argument to the first, which comes to
  the front, and quits (decision 0012).
- `cmake/`: shared CMake functions (`qgpui_add_qt_test`).
- `packaging/windows/`: the Inno Setup script (`qgpui.iss`) and `build-installer.ps1`, which runs
  `cmake --install` into `dist/stage` (ignored by git) and builds `dist/qgpui-<version>-setup.exe`.
  The version is the one in `project()`; `qgpui --version` prints it.
- `packaging/macos/`: `deploy-macos.sh` (a self-contained QGPUI.app, with the catalog in
  `Contents/Resources`), `sign-macos.sh`, `package-macos.sh` (the disk image),
  `notarise-macos.sh`, and `build-bundle.sh`, which runs the first three for `task bundle`.
- `.github/`: CI (`workflows/ci.yml`: build, test and installer on Windows, build, test and
  disk image on macOS, catalog tool tests on Linux, for pull requests and `master`) and releases
  (`workflows/release.yml`: a `v<version>` tag that matches `project()` publishes the installer
  and the notarized disk image, each with a fresh catalog inside; decisions 0019, 0022 and 0023).
  `actions/setup-windows-build` installs Qt and MSVC for the `windows-ninja` preset;
  `actions/setup-macos-build` installs Qt, selects Xcode 16 and gets Google's platform-tools. Lint workflow changes with `actionlint`.
- `docs/`: documentation in three folders, and the README's images; `docs/README.md` lists every document.
  - `docs/research/`: what was found out, with the evidence and the date: external systems
    (QuestPorts), the games' install processes, measurements on a real headset.
  - `docs/architecture/`: how QGPUI is built now (the overview, the recipe format). Keep these in
    step with the code.
  - `docs/decisions/`: one numbered record per decision (`NNNN-short-name.md`: status, date,
    context, decision, consequences). Do not edit an accepted record; write a new one that
    replaces it and says so, and change only the old record's status line to point to it.
  - `docs/images/`: screenshots for `README.md`. Retake them when the windows they show change.

## Documentation

`README.md` is for the app's users: what QGPUI does, what it needs and how to use it. Keep build
and developer instructions in `DEVELOPMENT.md`.

Put new documents in the folder that fits and add them to `docs/README.md`. Record a design
choice that someone could reasonably question as a new decision. Put measurements and findings
about external systems in research, and update architecture when the code changes.

## Build and test

- Qt is a prebuilt download in aqtinstall's layout, in the folder named by the `QT_ROOT`
  environment variable (it may be shared between projects), or else `.qt/`. The
  presets set `QGPUI_QT_SUBDIR` (for example `6.8.3/msvc2022_64`) and `CMakeLists.txt` adds the
  root. CMake caches the Qt it found: configure with `--fresh` after changing `QT_ROOT`.
- Presets: `windows` (Visual Studio 2022; use it locally), `windows-ninja` (needs a developer
  prompt), `macos`.
- macOS (decision 0020): CMake builds against the newest macOS 15 SDK, because Qt 6.8 draws flat
  push buttons with a newer one; configure with `--fresh` if the cache holds another SDK. Normal
  builds load Qt from `QT_ROOT`. `task bundle` (target `qgpui-bundle`,
  `packaging/macos/build-bundle.sh`) makes a self-contained, signed `QGPUI.app` and `qgpui.dmg`.
  The release signs with the Developer ID and notarizes in a separate job, from the secrets of the
  GitHub environment `release` (decision 0022). Mac-only UI code sits under
  `Q_OS_MACOS` in `MainWindow.cpp`. Check the UI on a Mac with `screencapture`: Qt's offscreen
  platform does not use the macOS style.
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
- `tst_gamestores_live` reads this PC's Steam and GOG libraries and matches every recipe against
  them, printing what it finds. It is skipped unless `QGPUI_STORES_LIVE=1`. Read its output with
  `ctest -V`.
- adb may not be on `PATH`. `qadb::AdbServer::findAdb()` also looks in the Android SDK, at
  `%LOCALAPPDATA%\Android\Sdk\platform-tools\adb.exe`.
- `CLAUDE.local.md` (ignored by git), when present, records where this machine keeps Qt, adb and
  other local checkouts.
- The QGPUI build copies adb (`adb.exe`, `AdbWinApi.dll`, `AdbWinUsbApi.dll`, `NOTICE.txt` on
  Windows) from `QGPUI_PLATFORM_TOOLS` (default: the Android SDK's platform-tools) into
  `platform-tools/` next to `qgpui.exe`, so users need no Android SDK. `findAdb()` looks there
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
