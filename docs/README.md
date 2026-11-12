# qgp documentation

The documents are in three folders.

## [`research/`](research)

What was found out, with the evidence: notes on external systems and on measurements. They record what was true when written, with the date.

- [QuestPorts data sources](research/questports-data-sources.md): where QuestPorts keeps its game list and install requirements, and how to read them.
- [Install processes](research/install-processes.md): the 145 games sorted by where their data comes from and how it reaches the headset.
- [How a Quest answers adb](research/quest-adb-behaviour.md): grants, pushes into `Android/data`, packages, storage, identity and keeping the screen on, measured on a Quest 3.

## [`architecture/`](architecture)

How qgp is built now. These documents change with the code.

- [Overview](architecture/overview.md): the data flow and the libraries.
- [Install recipes](architecture/install-recipes.md): the recipe format.

## [`decisions/`](decisions)

Why qgp is built this way. One numbered record per decision, with its context and consequences. A record is not edited once accepted; a new record replaces it and says so, and only the old record's status line changes to point to the new one.

| # | Decision |
|---|---|
| [0001](decisions/0001-catalog-from-payload-and-recipes.md) | Build a catalog from the payload and hand-written recipes |
| [0002](decisions/0002-picking-the-apk-release.md) | Pick the newest release that has a matching asset |
| [0003](decisions/0003-pushes-into-android-data.md) | Push into Android/data with mkdir, chmod and single files |
| [0004](decisions/0004-headset-identity.md) | Identify a headset by its hardware serial |
| [0005](decisions/0005-works-without-a-headset.md) | qgp works without a headset |
| [0006](decisions/0006-update-detection.md) | Detect updates from install records, then from versions |
| [0007](decisions/0007-finding-pc-installs.md) | Find PC installs by ID, folder name, then content |
| [0008](decisions/0008-install-records-on-the-headset.md) | Keep install records on the headset (replaces the storage part of 0006) |
| [0009](decisions/0009-files-folder.md) | Look for the user's files in one files folder, by game slug |
| [0010](decisions/0010-game-dialog-first.md) | Every action on a game starts in its game dialog |
| [0011](decisions/0011-checking-for-updates.md) | Check the headset's games for updates on their own |
| [0012](decisions/0012-single-instance.md) | Run one qgp per user; a second start activates the first |
| [0013](decisions/0013-keeping-the-headset-awake.md) | Keep the headset awake during installs, when the user asks |
| [0014](decisions/0014-keep-awake-button.md) | A toolbar button keeps the headset awake on demand |
| [0015](decisions/0015-sharing-the-keep-awake-override.md) | Leave another app's keep-awake alone, and release on exit |
| [0016](decisions/0016-checking-the-catalog-again.md) | qgp checks the catalog's paths and names again when it reads them |
| [0017](decisions/0017-other-ways-to-install.md) | The game dialog links to the port's own installers |
