# 0028: Custom games in the repository

- Status: accepted
- Date: 2026-11-13

## Context

QGPUI's catalog holds only QuestPorts games: the payload is the QuestPorts game list, and a recipe
without a game is left out. The author sometimes wants a game that is not on QuestPorts, either
their own or one to try out a recipe or a feature in QGPUI. QGPUI has no place for such a game.

## Decision

- **Custom games live in `data/custom-games/<slug>.json`**, one file per game, committed, following
  `data/custom-game.schema.json`. A file holds what QuestPorts would supply: the `port` row, and
  optionally `install`, `campaigns` and overrides of `resolved`.
- **Build-payload adds them to the payload**, after the QuestPorts games. They go through the
  site's own functions like a database row, so `resolved` is computed the same way. Each is marked
  `"custom": true`, and `sources.custom` lists them. Their recipes stay in `data/recipes/`, so
  build-catalog and QGPUI treat them like any other game.
- **Every custom slug starts with `qgpui-`.** The schema and build-payload both check it. A slug
  that is already taken fails the build, so a QuestPorts game with the same slug can never be
  replaced in silence.
- **They ship in releases.** The release builds its payload the same way. `--no-custom` leaves
  them out of a payload.

## Consequences

- A custom game needs no change to QGPUI or to the catalog format.
- A game kept only for testing ships in the next release unless its file is removed first.
- QuestPorts could one day use a `qgpui-` slug. Build-payload then fails instead of mixing the two
  games, and the custom game needs a new slug.
