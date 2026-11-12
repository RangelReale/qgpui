# 0001: Build a catalog from the payload and hand-written recipes

- Status: accepted
- Date: 2026-11-12

## Context

The QuestPorts payload says where each game's files end up, but not where they come from, in what order the steps run, or which release asset to take. That information exists only as prose in each game's install guide ([research](../research/install-processes.md)). QuestPorts' data also changes daily.

## Decision

qgp keeps its own install recipe per game in `data/recipes/<slug>.json`, written by hand and committed, following `data/recipe.schema.json`. A second command of the payload tool, `build-catalog`, checks the recipes and joins them with the payload into one catalog file, which qgp reads. Each recipe records the payload values it was written against (`checkedAgainst`), and `build-catalog` reports when they change.

## Consequences

- The merge happens at build time, so qgp reads one file and needs no Node or network to do it.
- Recipes need upkeep as QuestPorts changes; drift reports say which.
- qgp's reader rejects a raw payload with a message saying to build the catalog.
