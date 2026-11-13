# 0023: The files folder is optional

- Status: accepted
- Date: 2026-11-12
- Replaces: the default folder in [decision 0009](0009-files-folder.md)

## Context

[Decision 0009](0009-files-folder.md) gave the files folder a default, `<Documents>/qgpui/files`. QGPUI then used that folder even when the user had never chosen it. The game dialog offered Custom Files Folder, which created folders in Documents that the user did not ask for. Its guide told the user to put files in a folder they had never seen.

Many users need no files folder. Their games need only Steam or GOG installs, or files that they pick once in the game dialog.

## Decision

The files folder has no default. Until the user chooses one in File > Settings, there is no files folder:

- QGPUI looks only in the Steam and GOG libraries, and the user picks the other files with Choose.
- The game dialog hides Custom Files Folder.
- The game dialog's guide below each input says what is needed ("Needed: ..."), not where to put it. In place of the note on shared files, it says that a files folder can be set in File > Settings.

Clearing the field in Settings removes the files folder again. Everything else in decision 0009 is unchanged: the layout inside the folder and the order in which QGPUI looks.

## Consequences

- QGPUI writes nothing into Documents unless the user asks it to.
- A user who relied on the old default without setting it must now choose `Documents\qgpui\files` in Settings once.
- `gamestores::FilesFolder` treats an empty root as "no files folder": it finds nothing, and creating a game's folder fails.
