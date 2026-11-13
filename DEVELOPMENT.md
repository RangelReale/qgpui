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

### macOS

Use the `macos` preset (Ninja), with Qt in `$QT_ROOT/6.8.3/macos`:

```bash
cmake --preset macos -DQGPUI_PLATFORM_TOOLS=<platform-tools folder>
cmake --build --preset macos-debug
ctest --preset macos-debug
```

`QGPUI_PLATFORM_TOOLS` defaults to `~/Library/Android/sdk/platform-tools`. The app is
`build/macos/bin/Debug/qgpui.app`; `task run` opens it. It loads Qt from `QT_ROOT`, so it runs
only on the Mac that built it.

- Qt 6.8 needs a macOS 15 SDK: with a newer SDK it draws flat push buttons. CMake picks the newest
  installed one, such as the Command Line Tools' `MacOSX15.5.sdk`, and warns when there is none.
  Install the Command Line Tools for Xcode 16 if needed. Configure with `--fresh` after adding an
  SDK. See [decision 0020](docs/decisions/0020-macos-build.md).
- `task bundle` (the `qgpui-bundle` target, `packaging/macos/build-bundle.sh`) makes a copy that
  runs on other Macs, in `build/macos/bundle/Release`: `QGPUI.app`, with Qt inside it, and
  `qgpui.dmg`. It signs ad hoc, which Gatekeeper accepts only after the user allows the app in
  System Settings. Set `QGPUI_MACOS_SIGN_IDENTITY` to `Developer ID Application` to sign with your
  Developer ID, as a release does. Only the release workflow notarizes (see
  [CI and releases](#ci-and-releases)).

## The catalog

The app reads the catalog that `tools/questports-payload` builds. It needs Node 22 or later.

```bash
task payload
task catalog
```

`task payload` downloads the QuestPorts game list and install requirements into
`local/questports-payload.json`. `task catalog` joins them with `data/recipes/` into
`local/qgpui-catalog.json`.

The app takes the catalog from its command line, or else the file opened last, or else
`qgpui-catalog.json` in its data folder (`%APPDATA%\qgpui` on Windows,
`~/Library/Application Support/qgpui/qgpui` on macOS), or else the one the installer put next to
`qgpui.exe`. Development builds also look in `local/` in the repository, where the tool writes it
by default.

## Tests

Run tests through CTest, not the executables directly: CTest puts Qt's DLLs on `PATH`. Some tests
run against a real adb server, headset or game library, and are skipped unless an environment
variable turns them on:

- `tst_qadb_live`: `QADB_LIVE=1` for server requests, `QADB_LIVE_SERIAL=<serial>` for read-only
  device requests, and `QADB_LIVE_WRITE=1` to also push and pull a file in `/data/local/tmp`.
- `tst_gamestores_live`: `QGPUI_STORES_LIVE=1` reads this PC's Steam and GOG libraries and matches
  every recipe against them. Read its output with `ctest -V`.

## Installer

`packaging/windows/build-installer.ps1` builds the Windows installer from a Release build. It needs
[Inno Setup 6](https://jrsoftware.org/isinfo.php).

```bash
cmake --preset windows -DQGPUI_DEV_CATALOG=OFF
cmake --build --preset windows
powershell -File packaging/windows/build-installer.ps1 -BuildDir build/windows -Catalog local/qgpui-catalog.json
```

The script runs `cmake --install` into `dist/stage` (the app, Qt, the MSVC runtime and adb), adds
the catalog when `-Catalog` names one, and writes `dist/qgpui-<version>-setup.exe`. The version comes
from the `project()` call in `CMakeLists.txt`. Turn `QGPUI_DEV_CATALOG` off for a build you give to
others, because it builds the source folder's path into the app.

On macOS, the release's disk image comes from the scripts in `packaging/macos/`: `deploy-macos.sh`
adds Qt and the catalog to a copy of the app, `sign-macos.sh` signs it, `package-macos.sh` makes the
disk image and `notarise-macos.sh` notarizes it. The release workflow runs them (see
[CI and releases](#ci-and-releases)).

## CI and releases

GitHub Actions runs `.github/workflows/ci.yml` on every pull request and every push to `master`. It
builds and tests on Windows and macOS, builds the Windows installer and the macOS disk image
(without a catalog, kept for a week as the `qgpui-setup` and `qgpui-macos` artifacts), and runs the
catalog tool's tests and recipe checks. The CI disk image is signed ad hoc, so on another Mac it
opens only after **Open Anyway** in **System Settings > Privacy & Security**.

`master` is protected on GitHub. Every change reaches it through a pull request, and a pull request
can be merged only when both CI jobs pass. Nobody can push to `master` directly, force-push it or
delete it, and nobody can bypass these rules. No approving review is needed.

To release:

1. Raise the version in the `project()` call in `CMakeLists.txt` on a branch, open a pull request,
   and merge it when CI passes.
2. Update your `master`, tag the merged commit with the same version, and push the tag. Tags are
   not protected:

   ```bash
   git tag -a v1.0.0 -m "QGPUI 1.0.0"
   ```

   ```bash
   git push origin v1.0.0
   ```

`.github/workflows/release.yml` then builds a catalog from QuestPorts, builds and tests on Windows
and macOS, builds the Windows installer and the macOS disk image with the catalog, signs and
notarizes the disk image, and publishes a GitHub release with the installer, the disk image and the
catalog. It fails if the tag does not match the project version. To make a release again, run the
Release workflow by hand from its tag, under **Use workflow from**. See
[decision 0019](docs/decisions/0019-releases-and-the-installer.md) and
[decision 0022](docs/decisions/0022-macos-release.md).

The build jobs hold no secrets. A separate macOS job signs the app with a Developer ID and
notarizes the disk image, so no code the build ran can reach the certificate. It reads these
secrets from the GitHub environment `release`, and the release fails without them:

| Secret | What it holds |
| --- | --- |
| `MACOS_CERTIFICATE_P12` | The "Developer ID Application" certificate with its private key, exported from Keychain Access as a .p12 file, in base64: `base64 -i certificate.p12` |
| `MACOS_CERTIFICATE_PASSWORD` | The password of that .p12 file |
| `NOTARY_API_KEY` | An App Store Connect API key (the contents of its .p8 file), from **Users and Access > Integrations** |
| `NOTARY_API_KEY_ID` | That key's ID |
| `NOTARY_API_ISSUER` | The issuer ID shown above the keys |

Create the environment under **Settings > Environments**, and limit its deployment to tags `v*`, so
only a release can read the secrets.
