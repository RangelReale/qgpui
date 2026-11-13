# 0025: Find Epic installs from the launcher's manifests

- Status: accepted
- Date: 2026-11-13

## Context

Some users own recipe games on the Epic Games Store. The Epic Games Launcher writes one JSON manifest per installed game and add-on to `Data\Manifests\<id>.item`, in the folder that the registry value `HKLM\SOFTWARE\WOW6432Node\Epic Games\EpicGamesLauncher\AppDataPath` names (by default `C:\ProgramData\Epic\EpicGamesLauncher\Data\`). Each manifest holds `AppName` (the launcher's ID), `DisplayName`, `InstallLocation`, `bIsIncompleteInstall` and, for an add-on, the `MainGameAppName` of its game. The launcher keeps no local record of the games a user owns but has not installed.

On 2026-11-13, a search of the Epic store for the recipes' PC games found these six: Quake, Quake II, Doom 3, Morrowind, Amnesia: The Dark Descent and Tomb Raider I-III Remastered. Epic sells only the rereleases of Quake, Quake II and Doom 3, while their recipes ask for the original editions. Tomb Raider I-III Remastered is not the original Tomb Raider that the beefraiderxr recipe asks for. No search result said whether these rereleases hold the original files.

## Decision

`gamestores::EpicLibrary` reads the manifests and skips add-ons. `GameFinder` searches Epic installs in the same three passes as Steam and GOG installs (decision 0007). A recipe's `"store": "epic"` entry takes the manifest's `AppName` as its `appId`. A manifest marked incomplete counts as an incomplete install.

Only the openmw-vr (Morrowind) and sclerosis-vr (Amnesia) recipes list Epic. Their Epic products are the same editions that their Steam entries name. The other four recipes do not list Epic until someone checks an Epic install against them.

## Consequences

- An Epic install is found by its `detect` files even when its recipe does not list Epic, so a rerelease that holds the original files is still found.
- QGPUI cannot report Epic games that are owned but not installed, as it does for GOG Galaxy.
- The recipes list Epic without an `appId`, because the launcher's app names are not on the store's pages. Adding one needs a manifest from a real install.
- The manifest format is undocumented. A change shows up as Epic games no longer being found, not as an error.
