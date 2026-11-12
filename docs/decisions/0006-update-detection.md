# 0006: Detect updates from install records, then from versions

- Status: accepted; where the records are stored is replaced by [decision 0008](0008-install-records-on-the-headset.md)
- Date: 2026-11-12

## Context

The headset reports each app's `versionCode` and `versionName`; GitHub reports release tags. The two often line up (`1.0.1` and `v1.0.1`), but many tags carry no comparable version (`b003`, `n64`) or the game's name (`vcop2-v0.1.0`). Several ports also update themselves in the headset.

## Decision

`installer::updateStatus` decides in this order:

1. Not installed: NotInstalled.
2. qgp installed it, and the app's versionCode is unchanged since: the recorded release tag decides. The same tag is UpToDate; any other tag is UpdateAvailable, because [decision 0002](0002-picking-the-apk-release.md) always takes the newest release.
3. Otherwise: compare the release tag with the installed versionName by their longest dotted number. If either has none, the status is Unknown; qgp does not guess.

qgp writes an install record after each install: release tag, asset, versionCode and versionName, keyed by the headset's hardware serial ([decision 0004](0004-headset-identity.md)) and the package, in one JSON file.

An install that meets `INSTALL_FAILED_VERSION_DOWNGRADE` keeps the newer build and goes on. One that meets `INSTALL_FAILED_UPDATE_INCOMPATIBLE` (a changed signing key) stops and explains the backup, uninstall and restore it needs.

## Consequences

- Updates are exact for apps qgp installed, and best effort for others.
- Pre-release suffixes are ignored when comparing versions, so `0.3-beta` and `0.3` count as the same.
