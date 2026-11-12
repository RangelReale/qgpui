# 0010: Every action on a game starts in its game dialog

- Status: accepted
- Date: 2026-11-12

## Context

The main window lists the games as covers. The first action on a game, Create Files Folder ([decision 0009](0009-files-folder.md)), ran straight from the main window's Game menu and the covers' context menu. Installing adds more: finding the inputs, choosing missing files, checking the headset, and following the install's steps. Running these straight from the list would leave the user unable to see what an action will use before it starts.

## Decision

Opening a game (double-click, Enter, Game > Open, or the covers' context menu) shows its game dialog. The main window has no other action on a game. The dialog shows:

- the cover, the developer, how much qgp can do and whether the recipe was tested;
- whether the game is installed on the current headset, and which version;
- each input of the recipe, where qgp found it (Steam, GOG, the files folder or the user's choice) or that it is missing, with a Choose button;
- Open Files Folder, which does what Create Files Folder did, and Look Again;
- Install, which is enabled only when the recipe can be read, the headset is ready and every required input is found. Next to it, the dialog says what is missing.

While an install runs, the dialog shows the current step, a progress bar, the steps the user does in the headset with a Continue button, and a log. Closing the dialog asks whether to stop the install.

## Consequences

- Create Files Folder in the main window is replaced by Open Files Folder in the dialog. Decision 0009 is otherwise unchanged.
- The user sees which files an install will use, and where they came from, before it starts.
- One dialog per game: opening a game again brings its dialog to the front. Two games can be installed at the same time, on the same headset.
