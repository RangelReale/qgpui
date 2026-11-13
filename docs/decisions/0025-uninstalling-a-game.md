# 0025: Uninstalling a game

- Status: accepted
- Date: 2026-11-13

## Context

QGPUI could install games but not remove them. The user had to uninstall in the headset, and the
game's record stayed in `/sdcard/qgpui/installs.json` ([decision 0008](0008-install-records-on-the-headset.md)).

Uninstalling an app on the Quest also deletes its folders in `Android/data` and `Android/obb`.
For many ports these hold what the user copied in (game data, ROMs, disc images) and their saves.
Several recipes warn against uninstalling for this reason. `pm uninstall -k` keeps the app's
internal data and cache, but Android does not promise to keep the external `Android/data`
folders with it.

Decision 0008 suggests that `/sdcard/qgpui` could later hold save backups, which a changed signing
key also needs ([decision 0006](0006-update-detection.md)).

## Decision

- The game dialog shows Uninstall when the game's package is on the current headset, whether
  QGPUI installed it or not. Every action on a game starts in the game dialog
  ([decision 0010](0010-game-dialog-first.md)).
- Uninstall first asks for confirmation. The question names the game and the headset. It says
  that the game's `Android/data` and `Android/obb` folders go with it, that this cannot be undone,
  and that files elsewhere on the headset stay. Cancel is the default button.
- QGPUI runs `pm uninstall` without `-k`, the same as uninstalling in the headset.
- QGPUI then removes the package's install record from the headset's list and from the PC's
  cache with `installer::HeadsetRecordSync::remove`.
- The result shows in the install's banner: green when both steps worked, amber when the game is
  gone but its record is not, red when the uninstall failed.
- QGPUI does not back up saves before uninstalling. That is left for later, together with the
  backup a changed signing key needs.

## Consequences

- A user who uninstalls loses the files copied into `Android/data` and must copy them again on
  the next install. QGPUI copies them again as part of that install.
- A record whose game was uninstalled in the headset stays in the list until QGPUI uninstalls
  the game itself. The Installed filter reads the headset's packages, so the leftover record
  does not mark the game as installed.
- Not yet checked on a headset: that `pm uninstall` on the Quest deletes `Android/obb` as well as
  `Android/data`.
