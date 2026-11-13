# 0020: How QGPUI builds and ships on macOS

- Status: accepted; its bundle script and signing are replaced by [0022](0022-macos-release.md)
- Date: 2026-11-12

## Context

QGPUI was written on Windows. On 2026-11-12 it was built and run on a Mac with Qt 6.8.3, macOS 27 and its SDK. Four things went wrong:

- The link failed: Qt 6.8's OpenGL wrapper asks for the AGL framework, which the macOS 26 SDK and later no longer have.
- Push buttons were flat grey rectangles. An app linked against the macOS 26 SDK or later gets macOS 26's controls, and Qt 6.8 does not draw them correctly. The same code linked against the macOS 15.5 SDK showed normal rounded buttons. The `UIDesignRequiresCompatibility` Info.plist key did not change this.
- Without a deployment target, the app required the macOS version that built it.
- The build bundle loads Qt from the developer's Qt folder, so it runs only on that Mac.

## Decision

- CMake removes AGL from Qt's OpenGL link libraries on macOS.
- With Qt 6.8 or 6.9, CMake builds against the newest installed macOS 15 SDK, unless `CMAKE_OSX_SYSROOT` or `SDKROOT` names another. It warns when there is none. A later Qt that draws macOS 26's controls makes this unnecessary.
- The `macos` preset sets the deployment target to macOS 12, the oldest that Qt 6.8 supports.
- Normal builds do not run `macdeployqt`: they find Qt through their RPATH, as a developer's build should. A separate target, `qgpui-bundle` (`task bundle`), runs `apps/qgpui/make-bundle.sh`. It copies the app, adds Qt with `macdeployqt`, removes the SQL drivers other than SQLite, signs the bundle from the inside out and makes `qgpui.dmg`. `QGPUI_MACOS_SIGN_IDENTITY` names the certificate; the default, `-`, signs ad hoc. A real certificate also turns on the hardened runtime and a secure timestamp, which notarization needs. The bundled adb keeps Google's own signature.
- In the bundle, adb stays in `Contents/MacOS/platform-tools`, where `AdbServer::findAdb()` looks. `NOTICE.txt` goes to `Contents/Resources/platform-tools`, because codesign accepts only code in `Contents/MacOS`.
- The bundle's `Info.plist` names the app QGPUI, gives it an icon, and declares local network use. The adb server that QGPUI starts searches for headsets with mDNS and connects to them over Wi-Fi, and macOS counts that as QGPUI's network use.
- The Mac icon has its own set, `apps/qgpui/icons/macos/`: the Windows design on Apple's icon grid, rendered from `qgpui.svg` at 16 to 1024 pixels. The Windows PNGs stop at 256 pixels, which looks blurred in the Dock. On macOS the window icon is also the Dock icon, so the app loads this set at run time too, and the build makes `qgpui.icns` from it.
- The window follows macOS conventions where Qt does not on its own: an About item and a Window menu, the toolbars joined to the title bar, small controls in the filter bar so that it fits, and Return to open a game.

## Consequences

- A Mac build needs a macOS 15 SDK, such as the one in the Command Line Tools for Xcode 16, as long as QGPUI uses Qt 6.8.
- The disk image runs on other Macs, but an ad hoc signature makes Gatekeeper refuse it until the user allows it in System Settings. Shipping to other users needs a Developer ID certificate and notarization, which `make-bundle.sh` does not do yet.
- The bundle is built for the building Mac's architecture only.
- `macdeployqt` prints errors about the SQL drivers it cannot complete before the script removes them. They do not affect the result.
