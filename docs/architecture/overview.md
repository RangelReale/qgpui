# Architecture overview

qgp installs QuestPorts games on a Meta Quest from a Windows PC. This page shows how the parts fit together. Each part has its own README or header comments for the details.

## Data flow

```
QuestPorts Supabase (game list)  ─┐
QuestPorts GitHub repo (install   ├─► build-payload ─► local/questports-payload.json ─┐
  requirements, TypeScript)      ─┘                                                    ├─► build-catalog ─► local/qgp-catalog.json ─► qgp
data/recipes/<slug>.json (qgp's own, hand-written) ────────────────────────────────────┘
```

1. **`tools/questports-payload`, `build-payload`** (Node) reads the QuestPorts game list and install requirements and writes the payload. QuestPorts' requirements are TypeScript with computed parts, which is why this is a Node tool. See [research: QuestPorts data sources](../research/questports-data-sources.md).
2. **`data/recipes/`** holds one install recipe per game: where the APK comes from, what the user supplies, and the steps. The payload alone cannot drive an install. See [install recipes](install-recipes.md) and [decision 0001](../decisions/0001-catalog-from-payload-and-recipes.md).
3. **`build-catalog`** checks every recipe, joins them with the payload, and reports drift: payload values that changed since a recipe was written.
4. **qgp** reads the catalog.

## Libraries

| Library | Does | Depends on |
|---|---|---|
| `libs/qadb` | Talks to the adb server over TCP: devices, Wi-Fi pairing, shell, file sync, install, packages, storage, properties. | Qt Core, Network |
| `libs/questports` | Reads the catalog into `Catalog` and `Game`, with each game's recipe and drift. | Qt Core |
| `libs/gamestores` | Finds the user's Steam and GOG installs, and the GOG games they own, for a recipe's `pc-install` inputs, and looks for every input's files in the user's files folder. | Qt Core, Sql |
| `libs/installer` | Runs a recipe on a headset: fetches the APK, plans the copies, runs the steps; detects updates and keeps install records. | qadb, Qt Network |

`apps/qgp` is the Qt Widgets application built on them. `tools/adbmanager` is a developer window for trying qadb on a real headset.

## Installing a game

1. The user opens the game; its game dialog ([decision 0010](../decisions/0010-game-dialog-first.md)) reads the recipe from the catalog.
2. `gamestores::GameFinder` looks for the PC files that `pc-install` inputs need. `gamestores::FilesFolder` looks for every input's files in the user's files folder, one folder per game slug ([decision 0009](../decisions/0009-files-folder.md)). The user picks what neither finds (ROMs, disc images) in the dialog, which checks the names, endings and `detect` files.
3. `installer::ApkFetcher` picks the GitHub release ([decision 0002](../decisions/0002-picking-the-apk-release.md)) and downloads the APK into a cache.
4. `installer::Runner` runs the steps through `qadb`: installs, grants, launches, pushes ([decision 0003](../decisions/0003-pushes-into-android-data.md)), verifies, and pauses for steps the user does in the headset. If the user turned it on in File > Settings, `installer::KeepAwake` keeps the headset's screen on until the install ends ([decision 0013](../decisions/0013-keeping-the-headset-awake.md)), unless the toolbar's keep-awake button already does ([decision 0014](../decisions/0014-keep-awake-button.md)). qgp leaves an override that another app holds alone ([decision 0015](../decisions/0015-sharing-the-keep-awake-override.md)).
5. The app records what it installed in `/sdcard/qgp/installs.json` on the headset, with a copy on the PC keyed by the headset's hardware serial for when no headset is connected ([decisions 0004](../decisions/0004-headset-identity.md), [0006](../decisions/0006-update-detection.md) and [0008](../decisions/0008-install-records-on-the-headset.md)).
6. The dialog shows the result in a green, amber or red banner. Amber means the game is installed but its record is not.

Before installing, the dialog's Details button shows every step and command the install runs, with the files as they are found at that moment. `installer::planInstall` builds that list. The Runner builds its grant, launch, force-stop and folder commands with the same functions, so the list matches what runs.

## Updates

Each time the app reads the current headset, it checks the games on it for updates ([decision 0011](../decisions/0011-checking-for-updates.md)). `installer::ReleaseLookup` finds the newest release of each game's first APK, asking GitHub once per repository, and `installer::updateStatus` compares it with the package on the headset and its install record ([decision 0006](../decisions/0006-update-detection.md)). A game with an update is marked on its cover, and its game dialog offers Update instead of Install. An update is an ordinary install of the newer release.

## Without a headset

The catalog, the recipes and the PC-side checks work with no Quest connected, and with no adb. Every device query fails as an ordinary reply. See [decision 0005](../decisions/0005-works-without-a-headset.md).
