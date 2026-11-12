# 0015: Leave another app's keep-awake alone, and release on exit

- Status: accepted
- Date: 2026-11-12

## Context

[Decisions 0013](0013-keeping-the-headset-awake.md) and [0014](0014-keep-awake-button.md) override the Quest's proximity sensor with a broadcast. The override is one switch on the headset, shared by every app that uses it. An app on the headset, such as android-dev-settings, may already hold it and renew it every minute. When qgp then stopped, its release turned that app's override off too, until the app renewed it.

qgp sent its exit release from HeadsetConnection's destructor, which depended on the order in which Qt deletes the main window's children, and covered only the toolbar button, not a running install.

## Decision

- Before it overrides, `installer::KeepAwake` reads `dumpsys vrpowermanager`. When it says `Virtual proximity state: CLOSE`, another app holds the override: qgp sends nothing and releases nothing (Deferring). It reads again every minute and takes over when the state turns `DISABLED`. A failed read counts as no override.
- qgp sends a release only for an override it sent itself.
- `main()` calls `KeepAwake::releaseAll` right after the event loop ends, while the windows and the adb client still exist. It waits up to 2 seconds for the headsets to answer.
- The keep-awake button's tooltip and the status bar say when another app keeps the headset awake, and an install's log says so.

## Consequences

- Measured on a Quest 3 with android-dev-settings holding the override: qgp sent no broadcast while running and none on exit. With the override free, qgp took it within a second of starting and released it on exit.
- If the other app starts its override after qgp took it, qgp cannot tell the two apart and its release turns both off until the other app renews.
- A crash or a killed process sends no release; the override lapses some minutes later.
