# 0007: Find PC installs by ID, folder name, then content

- Status: accepted
- Date: 2026-11-12

## Context

About 50 games need files from a PC install ([research](../research/install-processes.md)). Steam and GOG can be read locally: Steam's `libraryfolders.vdf` and app manifests, and for GOG the registry and GOG Galaxy's database (see `libs/gamestores`). But recipes cannot always name a store ID: GOG packs install a game whose ID the GOG API does not list, some games are delisted, and copies may come from a CD or another store.

## Decision

`gamestores::GameFinder::find` matches a recipe's `pc-install` input in three passes: by the store IDs in its `stores`, then by install folder name, then by its `detect` files in every installed Steam and GOG game. It reports each folder once, as installed, incomplete (found by ID or name but unfinished or missing files) or, for GOG, owned but not installed. If nothing is found, the user picks the folder.

GOG Galaxy's SQLite database is read from a temporary copy, together with its write-ahead log, so Galaxy's own file is never opened.

## Consequences

- Recipe IDs are an optimisation and the key to ownership, not a requirement.
- Galaxy's schema is undocumented; a change shows as a warning, and installs are still found through the registry and by content.
