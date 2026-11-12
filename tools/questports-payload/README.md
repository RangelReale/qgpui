# questports-payload

Builds one JSON file that lists every QuestPorts game together with its install requirements. The
qgp app will read this file instead of contacting QuestPorts itself.

The data comes from two places, as described in
[`docs/research/questports-data-sources.md`](../../docs/research/questports-data-sources.md):

- **Game list:** the `ports` table in the QuestPorts Supabase database.
- **Install requirements:** `PORT_PACKAGE_CONFIGS` in `app/data/portPackageMap.ts` and
  `PORT_EXPANSIONS` in `app/data/expansions.ts`, in the QuestPorts source repository.

The two are joined by the game's `slug`.

## Why Node

The install requirements are TypeScript, and part of them is computed (for example
`MGS_DISC_LANGUAGES.map(...)`). They cannot be read reliably as text. The tool bundles the two data
files with esbuild and runs the result, so it gets exactly the values the site uses.

The files are downloaded from `raw.githubusercontent.com` at one commit, following the files'
imports. No clone and no `npm install` of the QuestPorts project is needed.

## Use

Requires Node 20 or later.

```bash
cd tools/questports-payload
npm install
npm run build-payload
```

This writes `local/questports-payload.json` at the root of the qgp repository. `local/` is not
committed.

Options (`node src/main.mjs --help`):

| Option | Default | Meaning |
|---|---|---|
| `--out <file>` | `local/questports-payload.json` | Output file. |
| `--repo <owner/name>` | `felipevc13/questports` | GitHub repository with the install data. |
| `--ref <ref>` | the default branch | Branch, tag or commit. |
| `--source <dir>` | | Read the install data from a local checkout instead of GitHub. |
| `--key <key>` | `QUESTPORTS_SUPABASE_KEY`, or the key on the site | Supabase publishable key. |
| `--supabase-url <url>` | `https://ccjteoxolasldhfgnoyx.supabase.co` | Supabase project URL. |
| `--site <url>` | `https://questports.com` | Page to read the publishable key from. |

Without `--key`, the tool reads the publishable key from the site's front page, because the site
owner can rotate it. Set `GITHUB_TOKEN` if the GitHub API rate limit (60 requests an hour) is a
problem; a run uses two requests.

The tool fails, and writes nothing, if a request fails, if the database returns fewer rows than it
reports, or if two rows share a slug. Games that exist in only one source are kept or skipped as
described below and listed as warnings.

## The qgp catalog

A second command joins the payload with qgp's hand-written install recipes in `data/recipes/`.
The recipe format is described in [`docs/architecture/install-recipes.md`](../../docs/architecture/install-recipes.md).

```bash
npm run build-catalog
```

This reads `local/questports-payload.json` and writes `local/qgp-catalog.json`. From the
repository root, `task catalog` does the same.

Options (`node src/build-catalog.mjs --help`):

| Option | Default | Meaning |
|---|---|---|
| `--payload <file>` | `local/questports-payload.json` | Payload to read. |
| `--recipes <dir>` | `data/recipes` | Recipe folder. |
| `--schema <file>` | `data/recipe.schema.json` | Recipe schema. |
| `--out <file>` | `local/qgp-catalog.json` | Output file. |
| `--strict` | off | Fail on the warnings below. |
| `--verbose` | off | Also list the games without a recipe. |

The command fails, and writes nothing, if any recipe breaks the schema or the checks in
`src/recipes.mjs`. It then reports what changed in the payload since each recipe was written:

- **Warning:** a recipe has no game, or the payload changed a recipe's `packageName` or
  `targetPath`. The recipe may install the wrong app or to the wrong place. With `--strict`, the
  command fails and writes nothing.
- **Note:** the payload changed a recipe's `version`. The recipe may still be right, but the new
  release is worth a look.

To clear a warning or a note, check the recipe against the new payload and update its
`checkedAgainst`.

## Tests

```bash
npm test
```

The tests use a small fixture repository in `test/fixtures/repo/` and fake HTTP responses. They do
not contact QuestPorts.

The tests also check every install recipe in `data/recipes/` against `data/recipe.schema.json`,
with the extra checks in `src/recipes.mjs`. The recipe format is described in
[`docs/architecture/install-recipes.md`](../../docs/architecture/install-recipes.md).

## Payload format

One JSON object, version 1:

```jsonc
{
  "format": "qgp-questports-payload",
  "version": 1,
  "generatedAt": "2026-11-11T12:00:00.000Z",
  "sources": {
    "database": { "url": "https://ccjteoxolasldhfgnoyx.supabase.co", "table": "ports", "rows": 145 },
    "repository": { "kind": "github", "repo": "felipevc13/questports", "ref": "main", "commit": "942cd71..." }
    // or { "kind": "local", "path": "C:\\src\\questports", "commit": "942cd71..." }
  },
  "originPlatforms": [ { "id": "pc", "label": "PC" }, { "id": "arcade", "label": "Arcade" } /* ... */ ],
  "vrStyles": [ { "id": "motion", "label": "Full VR (motion controls)" } /* ... */ ],
  "headsets": [ "Quest 1", "Quest 2", "Quest 3", "Quest 3S", "Quest Pro" ],
  "games": [
    {
      "slug": "avp-vr",
      "port": { /* the database row: all columns, unchanged */ },
      "install": { /* PORT_PACKAGE_CONFIGS[slug], or null */ },
      "campaigns": [ /* PORT_EXPANSIONS[slug], or null */ ],
      "resolved": {
        "installType": "apk_and_assets",
        "workflowType": "pc_assets",
        "requiresExternalFiles": true,
        "selfContainedSideload": false,
        "originPlatform": "pc",
        "vrStyle": "motion",
        "headsets": [ "Quest 2", "Quest 3", "Quest 3S", "Quest Pro" ],
        "noPcInstall": "no"
      }
    }
  ],
  "unmatched": {
    "portsWithoutInstall": [],
    "installWithoutPort": [],
    "campaignsWithoutPort": []
  }
}
```

- `games` holds one entry per database row, ordered by title and then slug.
- `port`, `install` and `campaigns` are copied from their sources unchanged. Their fields are
  listed in the data-sources document.
- `resolved` holds values computed by the site's own functions: `resolveInstallType`,
  `resolveWorkflowType`, `portRequiresExternalFiles` and `isSelfContainedSideload`. They fill in
  defaults for games whose install config is missing or incomplete. `originPlatform` is where the
  game comes from, as the site's catalog filter derives it (`portOriginPlatform` in
  `app/lib/portOrigin.ts`), or `null` when nothing matches. The other catalog filters are computed
  the same way: `vrStyle` (`portVrStyle`), `headsets` (`normalizeHeadsetList` of
  `supported_hardware`, in `app/lib/headsets.ts`) and `noPcInstall` (`app/lib/noPcInstall.ts`:
  `yes` when the headset alone can install the game, the site's "No PC needed", else `no` or
  `unknown`). The type, status and developer filters read `category`, `status` and `developer`
  from the row.
- `originPlatforms` lists the site's platforms (`ORIGIN_PLATFORMS`) in its order, with their labels.
  `vrStyles` (`VR_STYLES`) and `headsets` (`CATALOG_HEADSETS`) do the same for the VR style and
  headset filters. The catalog copies all three.
- `unmatched` lists slugs found in only one source. A game without an install config is still in
  `games`, with `install` set to `null`. An install config or campaign list without a database row
  is left out of `games`, because the site does not show that game.

Raise `version` when a change would break a reader of the previous format.

## Catalog format

One JSON object, version 1. Each game is the payload's game entry, unchanged, plus `recipe` and
`drift`:

```jsonc
{
  "format": "qgp-catalog",
  "version": 1,
  "generatedAt": "2026-11-12T10:00:00.000Z",
  "sources": {
    "payload": { "generatedAt": "2026-11-12T00:04:36.622Z", "sources": { /* the payload's sources */ } },
    "recipes": { "count": 5 }
  },
  "games": [
    {
      "slug": "xelee",
      "port": {}, "install": {}, "campaigns": null, "resolved": {},  // as in the payload
      "recipe": {
        "apks": [ /* ... */ ], "inputs": [ /* ... */ ], "steps": [ /* ... */ ]
      },
      "drift": [
        { "field": "version", "recipe": "v0.4.1-beta", "payload": "v0.5.0-beta" }
      ]
    }
  ],
  "unmatched": {
    "portsWithoutInstall": [], "installWithoutPort": [], "campaignsWithoutPort": [],
    "gamesWithoutRecipe": [ /* slugs */ ],
    "recipesWithoutGame": [ /* slugs */ ]
  }
}
```

- `recipe` is the recipe file without the keys that matter only in the file: `$schema`, `format`,
  `version`, `slug`, `checkedAgainst` and `notes`. It is `null` for a game without a recipe.
- `drift` lists the payload values that differ from the recipe's `checkedAgainst`. The fields are
  `packageName`, `version` and `targetPath`. A value that is not set is `null`.
- A recipe without a game is left out of `games` and listed in `unmatched.recipesWithoutGame`.
