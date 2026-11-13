# 0022: Release a notarized macOS disk image

- Status: accepted
- Date: 2026-11-12

## Context

Releases held only the Windows installer (decision 0019). The macOS bundle that decision 0020's `qgpui-bundle` target makes is signed ad hoc, so Gatekeeper refuses it on any other Mac until the user overrides it in System Settings. The developer's other apps, such as Icon Email Check, ship a disk image signed with a Developer ID and notarized by Apple, which opens without a warning.

## Decision

- A release also publishes `qgpui-<version>-macos.dmg`: QGPUI.app and a link to `/Applications`, to drag the one onto the other. The app is universal (Apple silicon and Intel) and includes the release's catalog.
- The app looks for the catalog it shipped with in `Contents/Resources`, because codesign accepts only code in `Contents/MacOS`. On Windows it stays next to `qgpui.exe`.
- The release workflow builds the catalog once, in a job of its own, and both platforms use it. The macOS build job builds, tests and deploys the app (`packaging/macos/deploy-macos.sh`), unsigned, and passes it on as a tar file. A separate job signs it from the inside out with the Developer ID, hardened runtime and secure timestamp (`sign-macos.sh`), makes the disk image (`package-macos.sh`), notarizes it with an App Store Connect API key and staples the ticket (`notarise-macos.sh`). Only that job reads the secrets, from the GitHub environment `release`, which only `v*` tags may use. It runs nothing from the build, and its actions are pinned to commits. This follows Icon Email Check's release.
- The release fails when the `release` environment lacks the secrets. It does not fall back to an ad hoc signature, because users would get an app that Gatekeeper refuses.
- With the Developer ID, adb is signed again with it, so that every executable in the app carries the same identity and the hardened runtime. Ad hoc builds keep Google's signature on adb.
- The bundle is named QGPUI.app, the app's name for users (decision 0018). The build's own bundle stays `qgpui.app`.
- CI builds, tests and packages on macOS too, signed ad hoc, and keeps the disk image as a workflow artifact, so that a broken Mac build shows before a release.
- The `qgpui-bundle` target runs the same scripts through `packaging/macos/build-bundle.sh`, which replaces `apps/qgpui/make-bundle.sh` from decision 0020.

## Consequences

- Publishing a release needs the five secrets in the environment `release`; DEVELOPMENT.md lists them.
- A release takes longer: the universal macOS build, and Apple's notarization, which usually takes a few minutes.
- The signing was tested on 2026-11-12 on a Mac with the Developer ID: every piece carried the identity, the hardened runtime and a timestamp, and the signed app started its adb and opened the catalog in `Contents/Resources`. Notarization itself was not tried before the first release.
