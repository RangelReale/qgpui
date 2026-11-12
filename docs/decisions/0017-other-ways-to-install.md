# 0017: The game dialog links to the port's own installers

- Status: accepted
- Date: 2026-11-12

## Context

Many ports ship their own installer: a Windows program, a `.bat` file or a build kit. It installs the APK and copies the game's files, and the port's guide often recommends it. A recipe follows one route, usually the guide's manual one, and the game dialog showed only that route.

This confused users. GalaxyQuest's guide offers a Windows installer that takes the Super Mario Galaxy disc image. qgp's recipe follows the manual route, so the dialog asked for "Cooked Super Mario Galaxy data". That is the output of a converter that the dialog named only in small text, with no link to the installer the guide recommends.

The payload's install guides named 15 such installers on 2026-11-12. For some games (gothic2-vr, gran-turismo-2-vr, harry-potter-vr, outlaws-vr, animal-crossing-vr) qgp cannot do a step that only the installer does. For others (gta-sa-vr-quest, vice-city-vr-quest, red-dead-redemption-vr, victory-heat-rally-vr) no APK is published and the build kit is the only route.

## Decision

A recipe may list `otherWays`: each has a label, an https link and a help text. The game dialog shows them in its header, highlighted, next to the original game's store. qgp does not run them.

The dialog's files guide also shows the help text of an input's `preparedWith` tool, and its web addresses are links.

## Consequences

- Users see every route the port offers and can pick one that needs fewer steps than qgp's, even when that means not using qgp for the install.
- A game installed this way shows as "Installed outside qgp". qgp still checks it for updates.
- The links are written by hand. A port that renames its installer needs the recipe's help text updated; links point to the release page, not to a file, so they keep working.
- `parseRecipe` rejects a link that is not https, because the dialog opens it in the browser.
