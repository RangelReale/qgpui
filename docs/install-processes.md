# Install processes in the QuestPorts payload

This note sorts the 145 games in the QuestPorts payload by how they are installed. Its purpose is to plan which install processes qgp must implement.

The findings come from `local/questports-payload.json`, generated on 2026-11-11. The note uses both the structured install data (`install`, `campaigns`, `resolved`) and the human install guide (`port.installation_guide`) of every game. The payload format is described in [`questports-data-sources.md`](questports-data-sources.md).

## Summary

The structured data says where files must end up: `targetPath`, `folders` and `criticalFiles`. It does not say where the files come from, how to prepare them, or in which order to do the steps. That information exists only in the Markdown guides.

So the payload alone cannot drive an installer. qgp needs its own install recipe per game, keyed by slug and layered over the payload.

## Where the game data comes from

Counts are approximate. Some games accept more than one source.

| Source | Games | Examples |
|---|---|---|
| Inside the APK, or downloaded by the app in the headset | ~20 | bugdom-vr, forsaken-vr, xonotic-vr, urbanterror-vr, rvgl-vr, wc-saga-vr |
| A PC install: a Steam, GOG or CD folder | ~50 | quakequest, doom3quest, sourcevr, openxw-vr, freespace2-vr |
| A cartridge ROM or an arcade set (MAME or NAOMI zip, Lindbergh dump) | ~40 | virtua-cop-vr, hotd-vr, goldeneye-vr, hotd4-vr |
| A disc image (GameCube, Wii, PS1, PS2, PSP, Xbox, Dreamcast) | ~35 | xelee, ratchet-and-clank-vr, metal-gear-solid-vr, nfsu-vr |
| Another Android game's APK or OBB | 4 | max-payne-vr, crazy-taxi-vr, gta-sa-vr-quest, red-dead-redemption-vr |
| Other dumps | 2 | astroquest (PS4 pkg), umbrella-chronicles-vr (Wii `common-key.bin`) |

No guide needs a GOG installer to be extracted. All guides use an installed folder. magic-carpet-vr reads GOG's `game.gog` files directly.

## How the data reaches the headset

The per-game table uses the names in bold.

1. **Import** (~45 games). Push the file to `/sdcard/Download/` or any folder. The user then picks it in the app, which imports and checks it. Nearly all ROM and disc games work this way.
2. **Tree** (~35 games). Push into a fixed `/sdcard/<Game>/` tree, for example `/sdcard/QuakeQuest/id1/`. This needs a mapping from source to destination, sometimes with renames:
   - openxw-vr: the folder `collectors_cd` becomes `XW94`.
   - quake2quest: the paks go flat into `/sdcard/Quake2Quest/`, with no `baseq2/` folder.
3. **Data** (~25 games). Push into `/sdcard/Android/data/<package>/files/`. The app must be launched once first, so that it owns the folder. goldeneye-vr, starship-vr and vice-city-vr-quest say not to create the folder by hand.
4. **Private** (2 games). The data goes into app-private storage through the app's own mechanism:
   - animal-crossing-vr uses a vendor Python helper, `device_data.py`.
   - openxwa-vr pushes a zip to `/sdcard/Android/obb/org.openxwa.quest/` and unpacks it with `adb shell content call ... --method unzip`. The guide says that files pushed directly belong to the wrong user.
5. **Script** (~15 games). A vendor tool on the PC converts the data, and often installs it as well. Examples: `INSTALL.bat` (gothic2-vr, harry-potter-vr, gran-turismo-2-vr, road-rash-jailbreak-vr), `Install on Quest.bat` (outlaws-vr), StarFoxAssetBuilder, `mgc-import`, `cook.py`, the Starship o2r generator. Most are Windows-only. None is described as able to run on the Quest.
6. **Build** (5 games). The APK is built on the user's PC. These need toolchains, a signing key that must be kept for updates, and in two cases a second Android device.
7. **None**. The APK is all that is needed.

## Steps that recur across games

### Getting the APK

- Most APKs come from GitHub releases, either at a fixed URL or from `releases/latest`.
- On `latest`, qgp must pick the right asset. Releases also carry Pico, PCVR and plain Android builds.
- primedgun and questcarnage are pinned to an older tag, because newer tags are PC-only builds.
- Some releases are zips that contain the APK and helper scripts.
- Some APKs need a manual download: itch.io, Patreon, Google Drive, projecthoneydew.com or SideQuest.
- crazy-taxi-vr publishes only an xdelta patch for a user-supplied APK.
- The version a guide names often differs from `port_download_url`. Trust the URL or the release API.

### Installing

- Always use `install -r`. Several guides warn that uninstalling deletes imported data or saves.
- freespace2-vr and fotg-vr use `-g`. galaxyquest uses `--no-incremental`.
- Some ports changed their signing key. An update then needs a save backup, an uninstall, a reinstall and a restore: nolf-vr, lba2-vr, twilight-princess-vr.
- sega-rally-vr's old v0.1.0 has a different package, which must be uninstalled first.
- questzdoom needs two APKs: the engine and the launcher.
- Read the package name from the APK. openmw-vr's file name says `com.openmw.vr`, but the package is `org.openmw.vr`.

### Permissions

- About 40 guides tell the user to grant "All files access" (`MANAGE_EXTERNAL_STORAGE`) in the headset.
- lba2-vr grants it over adb with `appops set <package> MANAGE_EXTERNAL_STORAGE allow`. openmw-vr uses `pm grant`.
- If `appops` works on the Quest for all of these, most steps in the headset disappear. This is not yet checked.

### Launching

- About 25 games must be launched once, to create folders or unpack bundled data.
- quake3quest needs two launches.
- Some games need a restart after the permission is granted.
- openmohaa-vr needs `am force-stop` after the push, and must then be started in the headset, not with `am start`.

### Shell tweaks

- `chmod`: descent-3-vr (`chmod 2777` on folders), ultima-4-vr (`666` on `*.SAV`).
- Marker files: wipeout-pure-vr's `files/no-hd-textures` skips a 450 MB download.
- Config edits: daggerfall-unity-vr (`settings.ini`), ctr-turbocharged-vr (`/sdcard/CTR/vr.cfg`).

### Checks on the PC before pushing

- Hashes are given for f-zero-x-vr and xrkart-64 (SHA-1), ocarina-of-time-vr (`supportedHashes.json`), and nfsu-vr and max-payne-vr (SHA-256).
- Many games need a specific region, revision or disc serial, such as GAFE01 Rev 0 or SCUS-94426.
- Accepted formats vary. Some games reject RVZ, zipped images or ISO files.
- Arcade zips must stay packed. Some sets must be complete; Rail Chase 2, for example, needs all 23 payloads.
- Free space on the headset matters. nfsu-vr needs up to 18 GB, fotg-vr and wc-saga-vr 9 GB, ratchet-and-clank-vr 6 GB.

### Checks after pushing

- `criticalFiles` lists files that must exist.
- quakequest, quake2quest and rtcwquest copy shareware or demo files on their first launch. A check on one file can then pass falsely, so the full file set must be required.
- USB transfer often cannot list `Android/data`. adb can, so qgp should verify over adb.

### Optional extras

- Campaigns and DLC: sourcevr, lambda1vr, razexr, doom3quest (`d3xp`), questsam, nolf-vr.
- Language folders: metal-gear-solid-vr maps the disc region to a folder under `/sdcard/languages/`.
- HD texture packs and mods: goldeneye-vr, perfect-dark-vr, majoras-mask-vr, sm64-coop-dx-vr, sega-rally-vr.

## Processes to develop

In order of games covered per unit of work:

1. **APK pipeline.** Resolve the release asset, download it, `install -r`, grant permissions over adb and launch once. Every game needs this. About 20 games are complete after it.
2. **Import a user file.** The user picks a ROM, disc image or zip. qgp checks the extension, size and hash or serial, then pushes the file to `Download` or to the target. This covers most of the ~75 ROM and disc games.
3. **Copy from a PC install.** Find Steam libraries (`libraryfolders.vdf`) and GOG installs (registry). Map source subfolders and globs to the target tree, push, and verify `criticalFiles`. This covers ~50 games, but each needs its own mapping.
4. **Android/data and app-private variants.** Order the launch-once step correctly, check file ownership, and support the content-provider unzip.
5. **Run a vendor tool.** Download the release's PC tool or script and run it with qgp's adb on `PATH`. Several scripts ship their own adb, which may fight with qgp's adb server.
6. **Guided manual.** Show the guide with links. This is the fallback for builds from source, Android dumps, Patreon or Drive downloads, Discord content and winlatorxr.

## Gaps and conflicts in the payload

- homeworld-unbound: the structured target is `Android/data`, but the guide pushes to `/sdcard/Download/`.
- questsam: the structured target is `Android/data`, but the guide uses `/sdcard/questsam/`.
- magic-carpet-vr, sclerosis-vr and wipeout-rewrite-vr have no usable target.
- sourcevr: the guide needs the `platform` folder, which the structured data leaves out.
- hexen2vr: `port_download_url` points to a README, not an APK.
- openmw-vr: the APK file name gives the wrong package name.
- lambda1vr: needs the `steam_legacy` beta branch in Steam. The guide reports the current build as broken.
- wave-race-64-vr: the guide is in Portuguese.
- Many guides name an older APK version than `port_download_url`.

## Per-game table

The rows follow the payload's order, by title. "Source" is where the data comes from. "Delivery" uses the names from [How the data reaches the headset](#how-the-data-reaches-the-headset).

| Slug | Source | Delivery | Notes |
|---|---|---|---|
| age-of-joy | Arcade ROMs and community cabinets | Manual | Cabinets come from Discord; the procedure is in an external guide. |
| avp-vr | PC install (Steam, GOG, CD) | Data | The first launch crashes, as expected. Optional soundtrack, renamed to `cd_tracks/trackNN.ogg`. |
| animal-crossing-vr | Disc, GAFE01_00 Rev 0 | Private | Vendor `device_data.py`. RVZ and zipped images are rejected. |
| armsx2-3d | Disc and PS2 BIOS | Import | BIOS and games folder are set in the app. May need other ARMSX2 builds uninstalled. |
| arx-libertatis-vr | PC install or the free demo | Tree | `/sdcard/ArxLibertatis/`. File names vary by language. |
| astroquest | PS4 dump, CUSA12392 v1.00 EU | Data | Needs a DualSense paired over Bluetooth. |
| beefraiderxr | PC install, Tomb Raider I | Tree | `/sdcard/BeefRaiderXR/`. Steam and GOG layouts differ. |
| behind-enemy-lines-vr | Arcade set `bel.zip` | Import | Keep zipped. |
| bomberman-hero-vr | N64 ROM, NA v1.0 | Import | |
| bugdom-2-vr | In the APK | None | |
| bugdom-vr | In the APK | None | |
| carnevil-vr | Arcade zip and CHD | Import | The CHD goes in a `carnevil/` subfolder. |
| citravr | 3DS ROM, decrypted | Tree | `/sdcard/CitraVR/roms/`. No pinned APK asset. |
| confidential-mission-vr | NAOMI zip, CHD and BIOS | Import | Keep the subfolder for the CHD. |
| crazy-taxi-vr | Android `.apkm`, version 6.0-80 | Script | Only an xdelta patch is published. |
| crisis-zone-vr | In the APK | None | Launch once to unpack. |
| csvr | PC install, Half-Life `cstrike` | Tree | `/sdcard/xash/cstrike/`. |
| ctr-turbocharged-vr | Disc, SCUS-94426 raw `.bin` | Tree | Renamed to `/sdcard/CTR/assets/ctr-u.bin`. |
| daggerfall-unity-vr | PC install, `arena2/` | Data | Check `settings.ini` afterwards. |
| death-crimson-ox-vr | NAOMI zip and BIOS | Import | |
| descent-1-2-vr | PC install, Descent 1 and 2 | Data | Two games in one app. A vendor `Setup.bat` also exists. |
| descent-3-vr | PC install, v1.4 data | Data | `chmod 2777` on folders. Never uninstall. |
| dolphinxr | Disc, GameCube or Wii | Import | Emulator configuration in the app. |
| doom3quest | PC install, `base/*.pk4` | Tree | `/sdcard/Doom3Quest/base/`. Optional `d3xp`. BFG Edition rejected. |
| f-zero-x-vr | N64 ROM, SHA-1 given | Import | |
| forsaken-vr | In the APK | None | 752 MB APK. |
| freespace2-vr | PC install | Tree | `/sdcard/FS2/`. Install with `-g`. |
| galaxyquest | Disc, Wii RMGP01 or RMGE01 | Script | Dolphin extraction and `cook.py`, then Data. About 7 GB. |
| generals-zero-hour-xr | PC install, Generals and Zero Hour | Import | A folder; picked in the app's setup assistant. Never uninstall. |
| ghost-squad-evolution-vr | Lindbergh dump, `vsg_l/` | Tree | One exact version only. |
| goldeneye-vr | N64 ROM | Import or Data | Data needs the name `ge.z64`. Do not pre-create the folder. |
| gothic2-vr | PC install | Script | `INSTALL.bat` packs a zip, which the app imports. |
| gran-turismo-2-vr | Disc, two PS1 discs | Script | `INSTALL-QUEST.bat`, then Data. The result cannot be listed over USB. |
| gran-turismo-3-vr | Disc, `.iso` only | Import | Pick the `quest` APK, not the `android` one. |
| gta-sa-vr-quest | Android Play splits and a mod pack | Build | Patches the user's APK. |
| vice-city-vr-quest | PC install | Build | Compiles reVC, then Data. |
| gunblade-ny-vr | Arcade set, Rev A | Import | `segabill.zip` if the set is split. |
| halocequest | Disc, Xbox | Import | Alternative: extracted `maps/` into `/sdcard/Documents/HaloCE/`. |
| harry-potter-vr | PC install, 2001 CD | Script | `INSTALL.bat`, then Data. |
| hexen2vr | PC install, `data1/` | Tree | `/sdcard/Hexen2VR/`. The APK source is not defined. |
| homeworld-unbound | PC install | Import | The structured target disagrees with the guide. Never uninstall. |
| iron-lung-vr | In the APK | None | Manual itch.io download. |
| jkxr | PC install, `base/*.pk3` | Tree | `/sdcard/JKXR/base/`. |
| jurassic-park-arcade-vr | Raw Thrills Linux dump | Build | Needs the Android NDK. |
| lambda1vr | PC install, Half-Life on `steam_legacy` | Tree | `/sdcard/xash/`. Campaigns. The current build is reported broken. |
| lets-go-jungle-vr | Lindbergh dump | Tree | `/sdcard/LetsGoJungle/`. One exact build. |
| lba2-vr | PC install | Tree | `/sdcard/lba2cc/`. `appops` grant. Signing key changed. |
| lupin-the-third-vr | NAOMI zip, CHD and BIOS | Import | |
| magic-carpet-vr | PC install, GOG `game.gog` | Script | `mgc-import bake`, then `/sdcard/mgcarpet/baked/`. |
| majoras-mask-vr | N64 ROM | Import | Never uninstall. Optional mods. |
| mario-party-4-partyboard | Disc | Import | Pick the `quest` APK. The app checks the hash. |
| strikers-charged-vr | Disc, R4QE01 rev 1 | Import | |
| max-payne-vr | Android APK and OBB, version 1.7 | Data | APK from Google Drive. |
| mega-man-64-vr | N64 ROM | Data | Exact file name. |
| metal-gear-solid-vr | Disc, two PS1 discs | Tree | Region decides the folder under `/sdcard/languages/`. Pick the Quest APK, not Pico. |
| mystical-ninja-vr | N64 ROM | Import | |
| nfsu2-vr | Disc, Xbox | Import | Needs 11–17 GB free. |
| nfsu-vr | Disc, Xbox | Import | `default.xbe` SHA-256 given. Up to 18 GB free. |
| nfl-blitz-2000-vr | N64 ROM, USA Rev 1 | Import | |
| ninja-assault-vr | NAOMI zip and BIOS | Import | |
| nolf-vr | PC install or two CDs | Tree | `/sdcard/nolf/`. Signing key changed. |
| ocarina-of-time-vr | N64 ROM or GameCube disc | Import | Hash list in `supportedHashes.json`. |
| openarena-vr | In the APK | None | |
| opencmr2-vr | PC install, zipped on the PC | Import | Import steps are only in an external README. |
| openmohaa-vr | PC install, `main/*.pk3` | Data | Extra `autoexec.cfg`, then `force-stop`. |
| openmw-vr | PC install | Tree or Data | `/sdcard/Morrowind/`. Package is `org.openmw.vr`. |
| opentie-vr | PC install, Steam | Tree | `/sdcard/OpenTIE/`, with folder renames. |
| openxvt-vr | PC install, Steam | Tree | `/sdcard/OpenXvT/`. |
| openxw-vr | PC install, Steam | Tree | `/sdcard/OpenXW/`, with folder renames. |
| openxwa-vr | PC install | Private | Zip and `content call`, or the vendor Windows launcher. |
| outlaws-vr | PC install, Remastered | Script | `Install on Quest.bat` downloads FFmpeg. |
| perfect-dark-vr | N64 ROM, NTSC Final | Import or Data | Pick the NTSC APK. |
| pikmin-2-vr | Disc, GPVE01 or GPVP01 | Import | APK from projecthoneydew.com. RVZ must be converted first. |
| pikmin-vr | Disc, USA Rev 1 | Import | Same as pikmin-2-vr. |
| virtua-cop-vr | Arcade sets | Import | |
| daytona-usa-vr | Arcade sets | Import | |
| dirt-dash-vr | Arcade sets | Import | |
| hotd-vr | Arcade set | Import | |
| sega-rally-vr | Arcade set | Import | Uninstall the v0.1.0 package first. Optional HD pack. |
| scud-race-vr | Arcade set | Import | |
| ppsspp-vr | PSP disc | Tree | `/sdcard/PSP/GAME/`. APK from ppsspp.org. |
| preyvr | PC install, Prey (2006) | Tree | `/sdcard/preyvr/preybase/`. |
| primedgun | Disc, NTSC-U Rev 0 | Tree | `/sdcard/PrimedGun/`, then picked in the app. Pinned tag. |
| metroid-prime-2-vr | Disc | Import | |
| quake2quest | PC install, `baseq2` paks | Tree | Flat in `/sdcard/Quake2Quest/`. Shareware false positive. |
| quake3quest | PC install, optional | Tree | `/sdcard/ioquake3quest/baseq3/`. Launch twice. |
| quakequest | PC install, `id1` paks | Tree | `/sdcard/QuakeQuest/id1/`. Shareware false positive. |
| qualyx | PC install, Half-Life: Alyx | Tree | `/sdcard/Qualyx/game/`. Very large. |
| strikers-quest-mr | Disc | Import | |
| questcarnage | PC install, `DATA` | Data | Pinned tag. |
| questcraft | Downloaded in the app | None | Microsoft account sign-in. |
| questzdoom | Shareware in the app, or PC WADs | Tree | `/sdcard/QuestZDoom/wads/`. Two APKs. |
| rail-chase-2-vr | Arcade set, Rev A | Import | All 23 payloads. |
| ratchet-and-clank-vr | Disc, two serials accepted | Import | 6 GB free; first setup up to 30 minutes. |
| rave-racer-vr | Arcade sets and `c71.bin` | Data | APK from itch.io. |
| razexr | PC install, three games | Tree | `/sdcard/RazeXR/`. |
| rvgl-vr | Downloaded in the app | None | |
| red-dead-redemption-vr | Android splits from a phone | Build | Signing key must be kept for updates. |
| re-code-veronica-x-vr | Disc, SLUS-20184 | Import | |
| survivor2-code-veronica-vr | NAOMI zip and BIOS | Import | |
| umbrella-chronicles-vr | Wii disc and `common-key.bin` | Tree | `/sdcard/UmbrellaChronicles/`. |
| road-rash-jailbreak-vr | Disc, PS1 SLUS-01053 BIN/CUE | Script | `INSTALL.bat`, then Data. |
| rtcwquest | PC install, `Main` | Tree | `/sdcard/RTCWQuest/Main/`. Demo false positive. |
| sclerosis-vr | PC install, Amnesia | Tree | "Root of internal storage". APK from itch.io. |
| questsam | PC install, TFE and TSE | Data or Tree | Guide and data disagree on the path. |
| sin-and-punishment-vr | N64 ROM | Import | |
| sm64-coop-dx-vr | N64 ROM, unmodified US | Import | Never uninstall. Optional mods. |
| smash-remix-vr | N64 ROM, optionally patched | Import | |
| sourcevr | PC install, HL2 and episodes | Tree | `/sdcard/SourceVRPort/common/`. Needs `platform`. |
| south-park-vr | N64 ROM | Import | |
| starfox-enhanced-vr | SNES ROM | Script | StarFoxAssetBuilder (Windows), then Data or picked in the app. |
| star-wars-arcade-vr | Arcade set `starwars.zip` | Data | Launch once first. |
| fotg-vr | Free download (Nebula), or Knossos | Tree | `/sdcard/FotG/FotG_R/`. Python script; 9 GB. |
| starship-vr | N64 ROM, Star Fox 64 | Script | o2r generator, then Data. |
| supertuxkart-vr | In the APK | None | |
| teraburst-vr | Arcade set | Import | |
| hotd2-vr | Disc, Dreamcast | Import | Also plays Maze of the Kings. |
| hotd4-special-vr | Lindbergh dump | Tree | `/sdcard/HouseOfTheDead4Special/`. |
| hotd4-vr | Lindbergh dump | Tree | `/sdcard/HouseOfTheDead4/`. |
| mok-vr | NAOMI zip, CHD and BIOS | Import | |
| simpsonshitrun | PC install | Tree | `/sdcard/SimpsonsHitRun/`. |
| time-crisis-2-vr | In the APK | None | |
| time-crisis-vr | In the APK | None | |
| twilight-princess-vr | Disc, GameCube or Wii | Import | Signing key changed in 2.3.0. |
| ultima-4-vr | PC install, free on GOG | Data | Case sensitive. `chmod` on saves. |
| unreal-gold-vr | PC install | Import | APK from Patreon. |
| ut99-vr-quest | PC install | Data or Tree | Downloads the 469 patch in the app. |
| urbanterror-vr | Downloaded in the app | None | |
| victory-heat-rally-vr | PC install, Steam | Build | `Install-Quest.bat` builds, signs and installs. |
| virtua-cop-2-vr | Arcade sets | Import | |
| vpinball-vr | Downloaded in the app | None | Optional own tables. |
| wakerquest | Disc, uncompressed ISO | Import | Never uninstall. |
| wave-race-64-vr | N64 ROM, USA Rev A | Import | |
| wiicompiled-vr-plus | Disc, PAL RMCP01 | Import | Built in the headset; rebuilt after each update. |
| wing-commander-2-vr | PC install or CD | Tree | `/sdcard/WingCommander2/`. |
| wc-saga-vr | Downloaded in the app | None | 9 GB. |
| wing-commander-vr | PC install, GOG | Tree | `/sdcard/WingCommander/`. |
| winlatorxr | Any Windows game | Manual | Wine container settings per game. |
| wipeout-pure-vr | Disc, PSP UCUS-98612 | Data | `import/` folder; errors in `SETUP-ERROR.txt`. |
| wipeout-rewrite-vr | Disc, PS1 NTSC | Script | `push_data.sh` from the repository. |
| wwf-no-mercy-vr | N64 ROM, NA Rev 1 | Import | |
| xwing-revival-vr | In the APK | None | |
| xelee | Disc, NTSC-U 1.02 | Import | |
| xonotic-vr | In the APK | None | 3 GB free. |
| xrkart-64 | N64 ROM, SHA-1 given | Import | Converted in the headset. |
