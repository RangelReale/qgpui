# QuestPorts data sources

This document explains where QuestPorts keeps its game catalog and the install data for each game. It also explains how a program can read that data.

The findings come from the live site (`https://questports.com`) and the QuestPorts source repository (`felipevc13/questports`, local checkout at `M:\prog\src\questports`, commit `942cd71`). They were checked on 2026-11-11.

## Summary

The data is split in two places.

| Data | Where it lives | Machine-readable |
|---|---|---|
| Game list, titles, descriptions, covers, versions, APK links | Supabase database, table `ports` | Yes |
| Install guide for people | Supabase, column `installation_guide` (Markdown) | No |
| One destination folder per game | Supabase, column `internal_storage_path` | Partly |
| Required files, target folders, accepted file types | Source code: `app/data/portPackageMap.ts` | Yes, but only in code |
| Campaigns and DLC folders | Source code: `app/data/expansions.ts` | Yes, but only in code |

The two halves are linked by the game's `slug`. For example, the database row with slug `avp-vr` matches the `avp-vr` entry in `PORT_PACKAGE_CONFIGS`.

There is no public API that returns the full catalog or the install requirements.

## The QuestPorts Android app

The Android app does not fetch a game list itself. It is a wrapper around a WebView (an embedded browser) that loads the QuestPorts website. The website loads the catalog and the install data.

- **Source:** the app's code is in a separate repository: `https://github.com/felipevc13/questports-app`. The QuestPorts repository has no Android project.
- **Start URL:** set in the Android project. It is most likely `https://questports.com`, but this was not confirmed.
- **Bridge:** the app adds a `window.QuestPortsApp` object to the page. It provides `installApk`, `cancel`, `isInstalled`, `getInstalledVersion` and `launch`. Newer builds may add `uninstall` and `getVersion`. The website's side of the bridge is in `app/lib/appBridge.ts`.
- **App detection:** the app adds `QuestPortsApp/<version>` to the browser's user-agent. The site uses this to know it is running inside the app.

## Reading the game list

There are three ways to read the game list. Supabase is the best one.

### Option 1: Supabase REST API (recommended)

The site's public pages include the Supabase project URL and a publishable key. A publishable key is meant to be visible in browsers. It is not an admin key. The database's row-level security rules (RLS, per-table access rules) decide what it can read.

- **Project URL:** `https://ccjteoxolasldhfgnoyx.supabase.co`
- **Key:** look for `sb_publishable_` in an inline `<script>` on any page of `https://questports.com`. The key is not copied here because the site owner can rotate it.

Example request:

```
GET https://ccjteoxolasldhfgnoyx.supabase.co/rest/v1/ports?select=*&order=title
apikey: <publishable key>
Authorization: Bearer <publishable key>
```

The API is PostgREST, so it supports column selection (`select=slug,title`), filters (`slug=eq.avp-vr`) and sorting (`order=title`). Add the header `Prefer: count=exact` to get the total in the `Content-Range` response header.

What the key can read:

| Table | Result |
|---|---|
| `ports` | Readable. 145 rows on 2026-11-11. |
| `port_verifications` | 401, not readable. |
| `analytics_events` | 401, not readable. |

The data is live. Edits in Supabase and the daily release sync (which updates `latest_version` and `port_download_url`) show up at once.

The table is not a promised API. The site owner can change the access rules or the columns at any time.

### Option 2: Prerendered page payloads

Nuxt is the framework the site is built with. The site prerenders every page at build time and saves the page's data next to it as `_payload.json`.

- **All games:** `https://questports.com/_payload.json`, key `data.ports`. Contains the list fields plus install counts and community vote totals.
- **One game:** `https://questports.com/ports/<slug>/_payload.json`, key `data["port-<slug>"]`. Contains the game's full record.

Both files also contain `data["port-verifications"]`: the approved verification reports for all games, not only the page's game. Filter it on `port_slug`. These reports cannot be read from Supabase with the publishable key.

Limitations:

- **Not plain JSON.** The files use the compact `devalue` format: one flat array where objects store indexes into that array instead of values. Decode them with `unflatten` from the `devalue` npm package.
- **Frozen.** The data is as old as the last site deploy.
- **Not stable.** The key names can change with any site update.

The site sometimes adds `?_b=<build id>` to these URLs as a cache buster. It is not required.

### Option 3: Site API routes

The site has a few server routes under `/api/`. Only one is useful for reading game data:

- `GET https://questports.com/api/ports/<slug>/community` returns plain JSON with review counts, the share of "works" votes and a per-headset breakdown.

`GET /api/ports` does not exist and returns 404. The other routes are for tracking, verification posts, the APK proxy and remote install.

## Columns of the `ports` table

All 27 columns, as returned by the Supabase REST API on 2026-11-11:

| Column | Notes |
|---|---|
| `id` | UUID |
| `slug` | Unique key. Links the row to the code configs. |
| `title`, `developer`, `developer_url`, `short_description` | Text |
| `category` | For example `source_port`, `emulator`, `decompilation` |
| `status` | For example `released`, `playable_beta` |
| `cover_image_url`, `youtube_video_id` | Media |
| `supported_hardware` | Array, for example `["Quest 2", "Quest 3"]` |
| `locomotion_types`, `has_6dof_controls` | Controls |
| `internal_storage_path` | One destination folder. Sometimes prose instead of a path. |
| `base_game_url`, `base_game_store` | Where to buy the original game |
| `port_download_url`, `port_download_source` | The APK link and a label such as `GitHub Releases (APK)` |
| `installation_guide` | Markdown guide for people |
| `troubleshooting_notes` | Text |
| `featured` | Boolean |
| `github_url`, `last_github_update`, `latest_version` | Release data, updated by the daily sync |
| `features` | Gameplay features, such as `stereo`, `refresh`, `haptics`. Nothing about installation. |
| `created_at`, `updated_at` | Timestamps |

`locomotion_types` and `base_game_store` are in the database but not in the page payloads. The payloads add `installs` and `community`, which are computed at build time.

The table has no column that lists required game files.

## Required game files

The machine-readable install requirements exist only in the QuestPorts source code. The website loads them as part of its compiled JavaScript and combines them with the database row in the browser.

### `app/data/portPackageMap.ts`

`PORT_PACKAGE_CONFIGS` maps each slug to a `PortPackageConfig`. It had 145 entries on 2026-11-11, one for each game in the database.

```ts
type InstallType = 'direct_apk' | 'apk_and_assets' | 'pc_builder_required'

interface PortPackageConfig {
  slug: string
  packageName: string          // Android package ID of the port's APK
  altPackages?: string[]       // Other package IDs used by older builds
  installType: InstallType
  workflowType?: PortWorkflowType
  criticalFiles: string[]      // Files that must exist for a valid install
  fileGuidance?: string        // Short instructions for people
  sourceStoreName?: 'Steam' | 'GOG' | 'Nintendo Wii Disc' | 'Itch.io' | 'Other'
  pcToolName?: string          // PC tool needed to prepare the files, if any
  targetPath?: string          // Main destination folder on the headset
  altPaths?: string[]          // Other accepted destination folders
  romExtensions?: string[]     // For emulators: accepted ROM file types
  romDirectories?: string[]    // For emulators: accepted ROM folders
  folders?: PortFolderRequirement[]
}

interface PortFolderRequirement {
  id: string
  name: string
  folderName: string
  targetPath: string
  altPaths?: string[]
  campaignId?: string          // Applies only to this campaign
  required: boolean
  expectedFiles?: string[]
  fileExtensionPattern?: string // Regular expression, for example '\\.(bin|cue)$'
  acceptance?: 'expected' | 'expected-or-extension' | 'any-file' | 'confirm'
  description?: string
}
```

`workflowType` is defined in `app/types/port.ts`:

| Value | Meaning |
|---|---|
| `direct` | The APK is all that is needed. |
| `pc_assets` | Needs files from a PC copy of the game (Steam, GOG, discs). |
| `obb_extractor` | Needs files extracted from an Android OBB file. |
| `smart_converter` | Needs a console game converted by a tool. |
| `emulator_roms` | An emulator that needs ROMs. |

`acceptance` decides when a folder counts as ready:

| Value | Rule |
|---|---|
| `expected` | Every file in `expectedFiles` must be present. |
| `expected-or-extension` | All expected files, or any file that matches `fileExtensionPattern`. |
| `any-file` | Any file in the folder is enough. |
| `confirm` | USB cannot check the folder, so the player confirms by hand. |

When `acceptance` is not set, `folderAcceptance()` uses `expected` if the folder has `expectedFiles` or `fileExtensionPattern`, and `any-file` otherwise.

Example entry (shortened):

```ts
'avp-vr': {
  slug: 'avp-vr',
  packageName: 'com.bassquake.quest.avpvr',
  altPackages: ['com.bassquake.avpvr'],
  installType: 'apk_and_assets',
  workflowType: 'pc_assets',
  criticalFiles: ['avp_huds/alien.rif', 'avp_rifs/temple.rif', 'fastfile/ffinfo.txt'],
  sourceStoreName: 'Steam',
  targetPath: '/sdcard/Android/data/com.bassquake.quest.avpvr/files/',
  folders: [
    {
      id: 'avp_huds',
      name: 'HUD models',
      folderName: 'avp_huds',
      targetPath: '/sdcard/Android/data/com.bassquake.quest.avpvr/files/avp_huds/',
      required: true,
      expectedFiles: ['alien.rif']
    },
    // ...
  ]
}
```

Useful helper functions in the same file:

| Function | Purpose |
|---|---|
| `getPortConfig(slug)` | Returns the config for a slug. |
| `resolveInstallType(port)` | Uses the config. Without one, it guesses from `port_download_url` and `base_game_url`. |
| `resolveWorkflowType(target, category)` | Resolves the workflow type. |
| `matchFolderRequirement(filePath, folder)` | Checks one file against a folder rule. |
| `folderAcceptance(folder)` | Returns the acceptance rule. |
| `folderCandidatePaths(folder)` | Returns every path to check, including the `/sdcard/` and `/storage/emulated/0/` forms of the same folder. |
| `foldersForCampaign(slug, campaign)` | Returns the folder rules for one campaign. |
| `portRequiresExternalFiles(cfg)`, `isSelfContainedSideload(cfg)` | Tell whether the APK alone is enough. |
| `installedPackageOnQuest(slug, packages)`, `isPortInstalledOnQuest(slug, packages)` | Match installed Android packages to a game. |

### `app/data/expansions.ts`

`PORT_EXPANSIONS` lists base campaigns and DLC for games that have them. It covers 7 games: `lambda1vr`, `sourcevr`, `doom3quest`, `quake2quest`, `razexr`, `qualyx` and `nolf-vr`.

```ts
interface PortCampaign {
  id: string
  name: string
  badge: string
  isBase: boolean
  folder: string          // Folder name on the headset
  fullPath: string        // Full destination path, for example '/sdcard/xash/valve/'
  steamPath?: string      // Matching folder in a Steam install
  exampleFiles: string    // Description of files to copy
  instruction: string     // Instructions for people
  storeUrl?: string | null
  storeName?: string | null
}
```

### Code that uses this data

- `app/components/QuestFileManager.vue` lists folders on the headset and marks expected files.
- `app/lib/catalogInstallFacts.ts` builds install facts from the config.
- `app/composables/useQuestAdb.ts` copies files to the headset over USB (WebUSB and ADB).

### Other per-game data in code

`app/data/` also has smaller per-slug files, such as `videoPreviews.ts`, `searchAliases.ts` and `recordedPortFeatures.ts`.

`app/data/mockPorts.ts` is an offline copy of the catalog with 97 games. The site uses it only when Supabase is unavailable. It is out of date and should not be used as a source.

## How the catalog is maintained

- **Changes go through migrations.** Catalog edits are written as SQL files in `supabase/migrations/`. The site owner applies them by hand in the Supabase SQL editor. On 2026-11-11, 17 of the 71 migrations inserted games. Replaying the migrations does not rebuild the exact live table, because the daily sync and manual edits also change rows.
- **Versions update daily.** `.github/workflows/sync-releases.yml` runs `scripts/sync-github-stats.js`, which updates `latest_version` and `port_download_url` from GitHub releases.
- **Pages update on deploy.** The site prerenders `/`, `/app`, `/library` and every `/ports/<slug>` at build time. Changes in Supabase reach the pages only after the next deploy.

## Getting the install data without the source code

Today, a client that does not have the QuestPorts source code cannot get the required file lists, except by scraping the site's minified JavaScript. That is fragile.

Two ways to publish this data:

1. **Server route.** Add a route to the QuestPorts site, such as `/api/ports/<slug>/install`, that returns `PORT_PACKAGE_CONFIGS[slug]` and `PORT_EXPANSIONS[slug]` as JSON. This is less work and keeps the data in one place.
2. **Database table.** Move the configs into a new Supabase table, such as `port_install_requirements`, readable with the publishable key. Clients could then read everything from Supabase, but the data would have to be kept in sync with the code or moved out of the code completely.
