# 0014: A toolbar button keeps the headset awake on demand

- Status: accepted
- Date: 2026-11-12

## Context

[Decision 0013](0013-keeping-the-headset-awake.md) keeps the headset awake only while an install runs. A user also wants the screen on at other times: while setting up a game in the headset, or across several installs. Two overrides on one headset would undo each other, because the first one to end sends the release.

## Decision

- An icon button next to Headset in the toolbar turns keep-awake on and off. It is off when qgp starts, and its state is not saved.
- `HeadsetConnection` holds it. It follows the current headset: a headset that becomes current and ready is kept awake, and one that stops being current is released. qgp releases it on exit, waiting up to 2 seconds for the headset to answer.
- While the button is on, an install does not start its own override, whatever the setting says. If the button is turned on during an install, the install's override stops without the release (`KeepAwake::abandon`). If it is turned off during an install and the setting is on, the install starts its own.
- A failed broadcast shows in the status bar.

## Consequences

- Turning the button off during an install sends the button's release and the install's new override on separate connections. If the release arrives last, the headset may sleep until the install renews the override, at most a minute later.
