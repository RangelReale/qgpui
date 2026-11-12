# 0016: qgp checks the catalog's paths and names again when it reads them

- Status: accepted
- Date: 2026-11-12

## Context

The catalog joins two sources that qgp does not control: the QuestPorts database (Supabase) and our recipes. `build-catalog` checks the recipes against `data/recipe.schema.json`, but the C++ readers trusted the file it writes. Several of its values reach a file system or a shell:

- A game's slug names its folder in the user's files folder, where qgp creates folders and writes a guide file. The site's slug was only required to be non-empty.
- A recipe's push and verify paths name files on the headset. The schema required only that they start with `/sdcard/`, so `/sdcard/../data/` passed. Below `Android/data` and `Android/obb`, qgp also runs `mkdir` and `chmod 2777` on the folders of those paths.
- An input's `root` and `detect` paths are joined to a game's install folder on the PC, and `from` picks the files to copy from it.
- The APK cache's file name came from GitHub's answer or from the URL, unchanged.

Anyone who can edit `local/qgp-catalog.json`, or change a row on the site, could make qgp read or write outside the folders it means to use.

## Decision

Check each value where it enters, at both ends:

- `build-catalog` rejects a site slug that is not a plain lower-case id. The schema's `headsetPath` rejects empty, `.` and `..` segments, backslashes and control characters. The new `relativePath` (for `root`, `detect` and `from`) rejects absolute paths, drives, `..` segments and control characters.
- In C++, `questports::parseCatalog` rejects an unsafe slug, and `installer::parseRecipe` applies the schema's rules again to ids, paths, package names, repositories, appops and permissions (`isSafeHeadsetPath`, `isSafeRelativePath`). `planPush` checks each final headset path, because it adds the PC's file names to the recipe's target. `ApkFetcher` cleans the cache's file names. `gamestores::FilesFolder` and `GameFinder` refuse slugs, ids, roots and detect paths that would leave their folder.

A bad slug fails the whole catalog, as a missing slug already did. `build-catalog` rejects it first, so only a hand-edited or damaged file gets that far.

The `shell` step stays as it is: it runs any command its recipe gives, as the headset's shell user. Each argument is quoted, so a value cannot add a second command, but the recipe chooses the command. Recipes are written and reviewed in this repository, so a shell step is trusted the same way as the code.

## Consequences

- A tampered catalog fails with an error that names the bad value, instead of writing outside the files folder or `/sdcard/`.
- The rules exist twice, in the schema and in C++. `tst_installer_recipe` reads every committed recipe, so a rule that is stricter in C++ fails a test.
- A push into `/sdcard/` itself stays allowed; `sclerosis-vr` uses one.
- Anyone who can change a recipe's `shell` step can still run commands on the headset. Limiting it to a list of commands would need a new decision.
