# 0009: Look for the user's files in one files folder, by game slug

- Status: accepted
- Date: 2026-11-12

## Context

99 recipe inputs ask for files that Steam and GOG cannot supply: 90 `file` inputs (ROM sets, disc images, files made with a PC tool) and 9 `pc-install` inputs without `stores` (arcade dumps, mod folders). [Decision 0007](0007-finding-pc-installs.md) covers only store installs. Without a standard place, the user picks every file by hand for every install and again for every update.

The inputs constrain where the files can go:

- Two file names are shared between games: `naomi.zip` (6 games) and `segabill.zip` (4 games). One copy should serve all of them.
- Many inputs accept any name with given extensions, such as a disc image. These can only be matched inside a folder that belongs to one game.
- Two games have several inputs that accept the same extensions: metal-gear-solid-vr (six language discs, `.cue` and `.bin`) and smash-remix-vr (two `.z64` ROMs). A game folder alone cannot tell these inputs apart.

## Decision

qgp has one files folder. Its default is `<Documents>/qgp/files`, and the user can change it. Inside it:

```
<files folder>/
  <slug>/              one folder per game, named by its slug
    <input id>/        optional: one folder per input
  any other folder/    for files that several games share, such as naomi.zip
```

For each input of a game, before it asks the user, qgp looks in this order:

1. **The input folder**, `<slug>/<input id>/`, when it exists. qgp then looks only there for that input.
2. **The game folder**, `<slug>/`, and every folder below it, except the folders of the game's other inputs.
3. **For exact file names only**, the rest of the files folder. This is where a shared file is found, even inside another game's folder.

A `file` input with `names` is found when every name is found, ignoring case. A `file` input with `extensions` takes every file with those extensions in step 1 or 2. It is ambiguous when it takes more than one file without `multiple`, or when it is found in the game folder and another input of the game accepts one of the same extensions. A `pc-install` input takes the input or game folder, or a folder up to two levels below it, that holds the input's `detect` files after its `root` is applied. It is ambiguous when two different folders match.

For a `pc-install` input with `stores`, the files folder is the last place qgp looks, after the store libraries. An ambiguous result is shown to the user to choose from; nothing is chosen for them.

The folders are named by slug, not by title. The slug never changes and is safe in a path; titles change and contain characters that Windows does not allow. To keep this easy for the user, the app's Create Files Folder action makes the selected game's folder and opens it in the file manager, so the user never types a slug. It creates input folders only where they are needed: for inputs that share an extension with another input of the game. An empty input folder would otherwise hide files that the user put in the game folder.

The same action writes `qgp - what goes here.txt` into the game folder of every game that asks for files. The guide lists each input with its label and help, where it goes, and what qgp recognises it by. It is rewritten each time, so it follows the recipe. qgp never takes it as an input's file and never pushes it to the headset.

## Consequences

- Most games need only the game folder. Only the two games with overlapping inputs need input folders, and their recipes' input IDs (`english`, `spanish`, `ssb`, `remix`) become names that users see. Renaming an input ID now breaks users' folders.
- A shared file needs to exist once. When it is found in several places outside the game folder, the first path in sorted order is taken.
- An update finds the same files again, so the user picks nothing twice.
- The search reads the file system only. It does not check hashes or disc headers; the installer still does that.
