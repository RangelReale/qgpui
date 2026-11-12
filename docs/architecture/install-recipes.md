# Install recipes

An install recipe tells QGPUI how to install one QuestPorts game on a headset. It holds only what the QuestPorts payload lacks:

- where the APK comes from;
- which files the user supplies;
- the steps to run, in order.

The payload already says where files end up. [`install-processes.md`](../research/install-processes.md) explains why it is not enough on its own.

Recipes are written by hand and committed, one file per game:

- `data/recipes/<slug>.json`: the recipes. The file name is the game's slug.
- `data/recipe.schema.json`: the JSON Schema they follow (draft 2020-12). Editors that read `$schema` check a recipe while you write it.

`tools/questports-payload` checks every recipe with `npm test`, or the given files with `npm run check-recipes -- <files>`. Its `build-catalog` command (`task catalog`) joins the recipes with the payload into `local/qgpui-catalog.json`, the file QGPUI will read. It also reports recipes without a game and recipes whose payload values have changed. The tool's README describes the command and the catalog format.

## Status

The format is a first draft (`version: 1`). It was designed from five sample games:

| Recipe | Kind |
|---|---|
| `quakequest` | Copy from a PC install into a fixed folder tree |
| `xelee` | A disc image, imported in the app |
| `starship-vr` | A file the user makes with a PC tool, pushed into `Android/data` |
| `virtua-cop-vr` | Arcade ROM sets, kept zipped |
| `supertuxkart-vr` | The APK only |

Every game in the payload now has a recipe: 114 `automatic`, 22 `guided` and 9 `manual`. They were written from the guides and the GitHub releases, checked against the schema, and resolved to a real APK where the source is GitHub. Most have not been installed on a headset. Each recipe's `notes` list what its author was unsure of.

Recipes used to carry a `status` (`draft`, `reviewed` or `tested`). It was removed on 2026-11-12: the game dialog no longer shows it, and the two tested recipes are recorded below.

Writing them added `support`, `inZip`, `multiple` and the `shell` step. Known gaps:

- Steps cannot be optional, so an optional marker file (wipeout-pure-vr) is left out.
- Backup and restore for ports that changed their signing key is described in `tell` steps and notes.

urbanterror-vr is the first recipe tested on a headset: the installer library's `Runner` installed it on a Quest 3 from its GitHub release, granted All files access with `appops`, and the game played.

ultima-4-vr is the first tested recipe with an input. On 2026-11-12 the QGPUI app's game dialog found Ultima IV in GOG Galaxy by its product ID, at `Ultima 4 - Quest of the Avatar` (the recipe had guessed `Ultima 4`). It installed release 0.2.15 on a Quest 3, pushed the 181 files (13 MB) into `Android/data`, made the `.SAV` files writable with `find ... -exec chmod`, and the game played.

How the runner treats pushes into `Android/data`, and why, is in [decision 0003](../decisions/0003-pushes-into-android-data.md); the measurements behind it are in [How a Quest answers adb](../research/quest-adb-behaviour.md).

## Fields

| Field | Meaning |
|---|---|
| `format`, `version` | Always `qgpui-install-recipe` and `1`. |
| `slug` | The game's slug. It must match the file name. |
| `support` | How much QGPUI can do. `automatic`: QGPUI does every step on the PC side; the user may still act in the headset. `guided`: the user must also run a PC tool or do a step QGPUI cannot, such as a manual download. `manual`: QGPUI installs what it can and shows the instructions, for example for ports the user builds from source. |
| `checkedAgainst` | The payload's `packageName`, `latest_version` and `targetPath` at the time the recipe was written. `build-catalog` reports when they change. |
| `packageName` | Optional. Overrides the payload's package name when the payload is wrong. |
| `requirements.freeSpaceMB` | Free space the headset needs. |
| `apks` | Where the APKs come from. The first one is the game's main app. |
| `inputs` | Files the user supplies. |
| `steps` | What QGPUI does, in order. |
| `otherWays` | Optional. Ways to install the game that QGPUI does not run, such as the port's own installer or build kit: a `label`, an https `url` and a `help` text each. |
| `notes` | For maintainers. Not shown to users. |

### Other ways to install

Many ports ship their own installer: a Windows program, a `.bat` file or a build kit that installs the APK and copies the game's files. A user who has read the port's guide expects it, and some of these installers need fewer steps than QGPUI's route. galaxyquest's installer, for example, takes the disc image itself, while QGPUI needs the folder that the converter makes from it. `otherWays` names these installers. The game dialog shows each one in its header with a link and the `help` text, so the user can choose. QGPUI never runs them.

`installer::parseRecipe` rejects a link that is not an https address, because the dialog opens it in the browser.

On 2026-11-12, the payload's install guides named 15 such installers: animal-crossing-vr, descent-1-2-vr, descent-3-vr (for the CDs), galaxyquest, gothic2-vr, gran-turismo-2-vr, gta-sa-vr-quest, harry-potter-vr, iron-lung-vr (SideQuest), openxwa-vr, outlaws-vr, red-dead-redemption-vr, road-rash-jailbreak-vr, vice-city-vr-quest and victory-heat-rally-vr. Scripts that only push files, as QGPUI does (questsam, questcarnage, lba2-vr), and developer scripts (openmw-vr, jurassic-park-arcade-vr, wipeout-rewrite-vr) are not listed.

### APK sources

- **`github`**: QGPUI takes the newest release of `repo` whose tag matches `tag` and that has an asset whose name matches `asset`, then that asset. A release without a matching asset is skipped, so a repository whose newest release leaves the game out, or is a PC-only build, still works. Pre-releases count only with `prerelease: true`. Both patterns are needed in practice:
  - Some repositories hold several games. port0r, for example, uses the tag `^vcop-v` for Cop VR.
  - Some repositories publish only pre-releases, so GitHub has no "latest" release. xelee and Starship-VR are examples.
  - Some releases carry Pico, PCVR or plain Android builds next to the Quest APK.
- **`url`**: a fixed download URL.

For `github` and `url`, `inZip` says the download is a zip and matches the path of the APK inside it.
- **`manual`**: the user downloads the APK from `page`, for example itch.io or Patreon, and picks the file. QGPUI checks the file name against `asset`.

### Inputs

Every input has an `id`, a `label` and an optional `help` text for the user. Users see some IDs as folder names in their files folder ([decision 0009](../decisions/0009-files-folder.md)), so do not rename an input once a recipe is published.

Inputs that share a `group` are alternatives: at least one of them is needed, and each is `required: false` on its own. metal-gear-solid-vr groups its six languages, descent-1-2-vr its two games.

An input may set `free` when anyone can get its files legally at no cost: a free release, a free store copy (Ultima IV on GOG, Daggerfall on Steam) or the port's own download. A game with an `automatic` recipe, whose APKs QGPUI downloads itself and whose required inputs (and each group) are free, counts as self-contained; the app's Show box can list just those.

**Open question: should QGPUI download free inputs?** QGPUI downloads APKs only; the user still fetches every free input. So "self-contained" means "needs nothing the user has to buy", while QuestPorts' `selfContainedSideload` means "needs nothing but the APK", and it is false for all six recipes with free inputs (arx-libertatis-vr, daggerfall-unity-vr, fotg-vr, openmohaa-vr, ultima-4-vr, unreal-gold-vr). Arx Libertatis VR, for example, runs on the free Arx Fatalis demo, but the user must find and unpack it. The QuestPorts payload has no download link for any game's data (checked 2026-11-12), so download URLs would have to go in our recipes. Doing this needs a download source for inputs (URL, unpacking, which files to keep), stable URLs from the original publisher or the port, and a choice of what self-contained means: only inputs that QGPUI downloads, or every free input. openmohaa-vr's `autoexec.cfg`, in the same GitHub release as the APK, is the easiest case.

An input may name a PC tool in `preparedWith`. QGPUI then links to the tool and tells the user how to use it. QGPUI does not run the tool itself.

- **`pc-install`**: a game installed on the PC.
  - `stores` lists where QGPUI looks: the store, its app ID and the usual install folder name. If QGPUI finds nothing, the user picks the folder.
  - `root` is the folder inside the install that the paths are relative to. For Quake it is `id1`.
  - `detect` lists files that must exist below `root` for the folder to count as the right one.
- **`file`**: one or more files the user picks.
  - `names` lists exact file names that are all required. `extensions` accepts one file of any name, or several with `multiple`, such as a `.cue` and its `.bin` files. An input needs `names` or `extensions`.
  - `keepArchived` means zips are pushed as they are, never unpacked.
  - `disc` checks a GameCube or Wii disc header (game ID and revision).
  - `sha1` lists accepted hashes.
  - `checkedByApp` means the game checks the file itself.

A folder that is not a store install, such as an arcade dump, is a `pc-install` input without `stores`.

### Steps

| Step | What QGPUI does |
|---|---|
| `install-apk` | Installs every APK with `adb install -r`, and with `-g` when `grantAll` is set. |
| `grant` | Grants an app op (`appops set <package> <op> allow`) or a permission (`pm grant`). |
| `launch` | Starts the main app and waits (`waitSeconds`, default 10). It then force-stops the app, unless `stop` is false. `why` says what the launch is for. |
| `push` | Pushes an input's files. Each `map` entry takes `from` and sends it `to` a path under `/sdcard/`. `from` is a glob below the input's root, or one of its `names`. Without `from`, every file of the input is pushed. A `to` that ends in `/` is a folder; any other `to` is the file's new path. A glob drops the folders before its first wildcard and keeps the rest of each path: `misc/**` to `/sdcard/X/misc/` sends `misc/a/b.txt` to `/sdcard/X/misc/a/b.txt`, and `*.pak` sends files flat. Below `/sdcard/Android/data/`, QGPUI creates folders with `mkdir -p`, pushes file by file and opens new folders to the app; see [decision 0003](../decisions/0003-pushes-into-android-data.md). |
| `verify` | Checks over adb that the listed files exist. |
| `shell` | Runs one command with `adb shell`; `args` holds the command and its arguments. Used for `chmod`, `content call`, `am force-stop` and marker files. `why` is required. Each argument is quoted, so the device shell expands no wildcards: use `find ... -name '*.SAV' -exec ...` instead of `chmod 666 *.SAV`. A non-zero exit code stops the install. |
| `tell` | Shows the user what to do in the headset. |

### Checks beyond the schema

`src/recipes.mjs` in the payload tool also checks:

- the file name matches the slug;
- `tag`, `asset` and `inZip` are valid regular expressions;
- `multiple` is used only with `extensions`;
- input IDs are unique, and every input is pushed;
- every push names an existing input, and a `from` matches one of that input's `names`;
- there is an `install-apk` step, and no `grant` or `launch` comes before it.

QGPUI does not trust the catalog it reads ([decision 0016](../decisions/0016-checking-the-catalog-again.md)). `installer::parseRecipe` checks the ids, headset paths, relative paths, package names, repositories, appops and permissions again, with the schema's rules, and `planPush` checks each final headset path. Headset paths must stay below `/sdcard/`: no empty, `.` or `..` segment, no backslash and no control character. Relative paths (`root`, `detect`, `from`) must not be absolute, name a drive or contain `..`.
