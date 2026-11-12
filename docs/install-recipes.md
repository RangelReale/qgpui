# Install recipes

An install recipe tells qgp how to install one QuestPorts game on a headset. It holds only what the QuestPorts payload lacks:

- where the APK comes from;
- which files the user supplies;
- the steps to run, in order.

The payload already says where files end up. [`install-processes.md`](install-processes.md) explains why it is not enough on its own.

Recipes are written by hand and committed, one file per game:

- `data/recipes/<slug>.json`: the recipes. The file name is the game's slug.
- `data/recipe.schema.json`: the JSON Schema they follow (draft 2020-12). Editors that read `$schema` check a recipe while you write it.

`tools/questports-payload` checks every recipe with `npm test`, or the given files with `npm run check-recipes -- <files>`. Its `build-catalog` command (`task catalog`) joins the recipes with the payload into `local/qgp-catalog.json`, the file qgp will read. It also reports recipes without a game and recipes whose payload values have changed. The tool's README describes the command and the catalog format.

## Status

The format is a first draft (`version: 1`). It was designed from five sample games:

| Recipe | Kind |
|---|---|
| `quakequest` | Copy from a PC install into a fixed folder tree |
| `xelee` | A disc image, imported in the app |
| `starship-vr` | A file the user makes with a PC tool, pushed into `Android/data` |
| `virtua-cop-vr` | Arcade ROM sets, kept zipped |
| `supertuxkart-vr` | The APK only |

Every game in the payload now has a recipe: 114 `automatic`, 22 `guided` and 9 `manual`. All are `draft`: written from the guides and the GitHub releases, checked against the schema, and resolved to a real APK where the source is GitHub, but not installed on a headset. Each recipe's `notes` list what its author was unsure of.

Writing them added `support`, `inZip`, `multiple` and the `shell` step. Known gaps:

- An input cannot say "at least one of these". metal-gear-solid-vr marks all six languages optional.
- Steps cannot be optional, so an optional marker file (wipeout-pure-vr) is left out.
- Backup and restore for ports that changed their signing key is described in `tell` steps and notes.

## Tested on a headset

Checked on a Quest 3 with Android 14 (build 52641960034500520) on 2026-11-12, using an installed port (galaxyquest) and test files that were removed afterwards.

- **`grant` with `appops` works.** `appops set <package> MANAGE_EXTERNAL_STORAGE allow` succeeds over adb, and `appops get` then reads `allow`. Android checks this app op when an app asks whether it has All files access, so the recipes' `grant` steps can replace the tap in the headset. The `tell` fallbacks can stay for apps that ask again.
- **Outside `Android/data`, `adb push` works as usual,** including a whole folder. Files there go through Android's storage layer, which gives apps access.
- **Inside `/sdcard/Android/data/<package>/`, the rules differ.** The app's own folders belong to the app (`drwxrws---`, owner the app's user). What adb creates belongs to the shell user (2000):
  - A single file pushed into a folder that exists works, and the file is readable by everyone (`-rw-r--r--`).
  - A folder that adb creates is `drwxrws---`, so the app cannot enter it.
  - `adb push` of a whole folder fails (`failed to read copy response: EOF`), and so does a file push that must create more than one folder level (`secure_mkdirs failed: Operation not permitted`).
  - `adb shell mkdir -p` creates nested folders, and `chmod` on them works.
  - galaxyquest's own install script leaves its pushed folders `drwxrwxrwx`, and the game runs with them; descent-3's guide uses `chmod 2777` for the same reason.

The installer library's `Runner` then installed urbanterror-vr on the same headset (`tst_installer_live`): it picked release v1.0.1 on GitHub, downloaded the 11 MB APK, installed it and granted `MANAGE_EXTERNAL_STORAGE`, which `appops get` read back as `allow`. The game itself was not started, so the recipe stays `draft`.

So the recipe runner must, for every push below `/sdcard/Android/data/`: create the folders with `mkdir -p`, push the files one at a time, and open each folder it created to the app (`chmod 2777`). The `launch` step before such a push stays necessary, because the app must create its own `files` folder.

## Fields

| Field | Meaning |
|---|---|
| `format`, `version` | Always `qgp-install-recipe` and `1`. |
| `slug` | The game's slug. It must match the file name. |
| `status` | `draft` means written from the guide. `reviewed` means checked by a person. `tested` means installed on a real headset. |
| `support` | How much qgp can do. `automatic`: qgp does every step on the PC side; the user may still act in the headset. `guided`: the user must also run a PC tool or do a step qgp cannot, such as a manual download. `manual`: qgp installs what it can and shows the instructions, for example for ports the user builds from source. |
| `checkedAgainst` | The payload's `packageName`, `latest_version` and `targetPath` at the time the recipe was written. `build-catalog` reports when they change. |
| `packageName` | Optional. Overrides the payload's package name when the payload is wrong. |
| `requirements.freeSpaceMB` | Free space the headset needs. |
| `apks` | Where the APKs come from. The first one is the game's main app. |
| `inputs` | Files the user supplies. |
| `steps` | What qgp does, in order. |
| `notes` | For maintainers. Not shown to users. |

### APK sources

- **`github`**: qgp takes the newest release of `repo` whose tag matches `tag` and that has an asset whose name matches `asset`, then that asset. A release without a matching asset is skipped, so a repository whose newest release leaves the game out, or is a PC-only build, still works. Pre-releases count only with `prerelease: true`. Both patterns are needed in practice:
  - Some repositories hold several games. port0r, for example, uses the tag `^vcop-v` for Cop VR.
  - Some repositories publish only pre-releases, so GitHub has no "latest" release. xelee and Starship-VR are examples.
  - Some releases carry Pico, PCVR or plain Android builds next to the Quest APK.
- **`url`**: a fixed download URL.

For `github` and `url`, `inZip` says the download is a zip and matches the path of the APK inside it.
- **`manual`**: the user downloads the APK from `page`, for example itch.io or Patreon, and picks the file. qgp checks the file name against `asset`.

### Inputs

Every input has an `id`, a `label` and an optional `help` text for the user.

An input may name a PC tool in `preparedWith`. qgp then links to the tool and tells the user how to use it. qgp does not run the tool itself.

- **`pc-install`**: a game installed on the PC.
  - `stores` lists where qgp looks: the store, its app ID and the usual install folder name. If qgp finds nothing, the user picks the folder.
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

| Step | What qgp does |
|---|---|
| `install-apk` | Installs every APK with `adb install -r`, and with `-g` when `grantAll` is set. |
| `grant` | Grants an app op (`appops set <package> <op> allow`) or a permission (`pm grant`). |
| `launch` | Starts the main app and waits (`waitSeconds`, default 10). It then force-stops the app, unless `stop` is false. `why` says what the launch is for. |
| `push` | Pushes an input's files. Each `map` entry takes `from` and sends it `to` a path under `/sdcard/`. `from` is a glob below the input's root, or one of its `names`. Without `from`, every file of the input is pushed. A `to` that ends in `/` is a folder; any other `to` is the file's new path. A glob drops the folders before its first wildcard and keeps the rest of each path: `misc/**` to `/sdcard/X/misc/` sends `misc/a/b.txt` to `/sdcard/X/misc/a/b.txt`, and `*.pak` sends files flat. Below `/sdcard/Android/data/`, qgp creates folders with `mkdir -p`, pushes file by file and opens new folders to the app; see [Tested on a headset](#tested-on-a-headset). |
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
