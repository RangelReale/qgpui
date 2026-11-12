# 0008: Keep install records on the headset

- Status: accepted; replaces where [decision 0006](0006-update-detection.md) stored its records
- Date: 2026-11-12

## Context

Decision 0006 kept qgp's install records in one JSON file on the PC. A user who installs from one PC and checks for updates from another, or reinstalls qgp, would lose them. Tools that manage Quest apps from outside, such as Beat Saber mod managers, keep their state in a folder on the headset's shared storage.

## Decision

Each headset holds its own list of what qgp installed there, at `/sdcard/qgp/installs.json` (`installer::kHeadsetRecordsPath`). The PC keeps a copy, keyed by the headset's hardware serial ([decision 0004](0004-headset-identity.md)), only as a cache for when no headset is connected.

`installer::HeadsetRecordSync` keeps the two in step: it reads the headset's list, joins it with the cache (per package, the record installed last wins, and a newly installed record always replaces the old one), writes the headset's list only when it changed, and updates the cache. It never overwrites a list on the headset that it cannot parse.

The folder name is plain, not hidden, so a user who finds it can tell what it is.

## Consequences

- Records follow the headset between PCs and survive reinstalling qgp. A factory reset removes them together with the apps they describe.
- The user can delete the file, from the headset's Files app for example. qgp then falls back to comparing versions, as for apps it did not install.
- The folder survives uninstalling apps, so it can later hold save backups for apps whose signing key changed.
- Records go stale when an app is removed or updated outside qgp. The update check already guards against this: a record counts only while the app's versionCode matches.
