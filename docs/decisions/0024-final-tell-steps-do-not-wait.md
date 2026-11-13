# 0024: An install does not wait for its final tell steps

- Status: accepted
- Date: 2026-11-13

## Context

A `tell` step shows the user what to do in the headset, and the install paused on it until the
user pressed Done, Continue. Of the 140 tell steps in the recipes, 135 are the recipe's last
step: "open the game from Library > Unknown Sources" and similar. Nothing runs after them, so the
pause only held back the result, and it was easy to miss that the install was waiting. The other
5 come before the install and warn about an older build (uninstall it, back up saves first).
Those must pause.

## Decision

- A tell step followed only by tell steps does not pause. `installer::Runner` emits
  `afterInstall()` with its text and goes on. The game dialog shows the text in the result
  banner, below "Install complete", as "Now, in the headset: ...".
- A tell step with other steps after it still pauses. The game dialog shows it in an amber
  banner headed "The install is waiting for you", stops the progress bar, focuses Done,
  Continue and flashes the taskbar.
- Recipes do not change. Where a tell step sits decides whether it pauses.

## Consequences

- An install ends, and is recorded, as soon as the last step that does something ends.
- A recipe that needs the user to act before a later step must put that later step after the
  tell step. That is already how the 5 pausing tell steps are written.
