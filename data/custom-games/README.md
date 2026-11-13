# Custom games

Games that are not on QuestPorts, kept in this repository. Use them to add a game of your own, or
to try something out in QGPUI. `npm run build-payload` (`task payload`) adds every file here to the
payload, after the QuestPorts games, and `build-catalog` joins them with their recipes like any
other game.

Each file is named `<slug>.json` and follows [`custom-game.schema.json`](../custom-game.schema.json).
Every slug starts with `qgpui-`, so that it never clashes with a QuestPorts game. Build-payload
fails, and writes nothing, if a file breaks the schema or if its slug is already taken.

```jsonc
{
  "$schema": "../custom-game.schema.json",
  "format": "qgpui-custom-game",
  "version": 1,
  "slug": "qgpui-my-game",
  // Takes the place of the game's row in the QuestPorts ports table, with the same column names.
  "port": {
    "title": "My Game",
    "developer": "Me",
    "category": "source_port",
    "status": "playable_beta",
    "supported_hardware": ["Quest 3", "Quest 3S"],
    "latest_version": "v1.0.0",
    "installation_guide": "### Overview\nMarkdown, as on a QuestPorts page."
  },
  // Takes the place of the game's PORT_PACKAGE_CONFIGS entry. Optional.
  "install": { "packageName": "com.example.mygame" },
  // Overrides what the site's functions compute from port and install. Optional.
  "resolved": { "originPlatform": "pc", "vrStyle": "motion", "noPcInstall": "no" }
}
```

The payload entry is `slug`, `port` (with `slug` filled in), `install` and `campaigns` (`null` when
left out) and `resolved`, as for a QuestPorts game, plus `"custom": true`. The payload's
`sources.custom` lists the custom games it holds.

To install the game, write its recipe in [`data/recipes/`](../recipes/) as for any other game, with
the same slug. Its `checkedAgainst` takes `install.packageName` and `port.latest_version` from this
file.

A release builds the payload with these games, so they ship in the release's catalog. To leave them
out of a payload, run `npm run build-payload -- --no-custom`.
