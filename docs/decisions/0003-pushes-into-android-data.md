# 0003: Push into Android/data with mkdir, chmod and single files

- Status: accepted
- Date: 2026-11-12

## Context

About 25 recipes copy game data into `/sdcard/Android/data/<package>/`. On a Quest 3, adb cannot create nested folders there, cannot push a whole folder there, and the folders it does create are closed to the app ([research](../research/quest-adb-behaviour.md)).

## Decision

For every push below `/sdcard/Android/data/` or `/sdcard/Android/obb/`, `installer::Runner`:

1. creates the missing folders, parents first, with `mkdir`, and opens each one it created to the app with `chmod 2777`;
2. pushes the files one at a time, with mode 0666.

Recipes still launch the app once before such a push, so that the app creates its own `files` folder. Recipes do not spell out these steps; the runner applies them to every such push.

## Consequences

- Recipes stay simple, and a recipe author cannot forget the rule.
- Files pushed this way belong to the shell user and are open to every app. That matches what vendor install scripts do.
- descent-3-vr's own `chmod 2777` step is now redundant but harmless.
