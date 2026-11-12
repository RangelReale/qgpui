# 0012: Keep the headset awake during installs, when the user asks

- Status: accepted
- Date: 2026-11-12

## Context

A Quest sleeps about 20 seconds after it is taken off. An install can take minutes, and a user often lays the headset down while the PC copies files. A sleeping headset leaves tell steps waiting, and may drop a wireless adb connection. The only way found to prevent this is Meta's undocumented `prox_close` broadcast, which overrides the proximity sensor (see [How a Quest answers adb](../research/quest-adb-behaviour.md#keeping-the-screen-on)).

## Decision

- File > Settings has the setting "Keep the headset's screen on during installs". It is off by default, because it uses an undocumented broadcast and keeps the display lit.
- When it is on, `installer::KeepAwake` sends `prox_close` when an install starts and again every minute. It sends `automation_disable` when the install ends, fails or is stopped.
- A failed broadcast is written once to the install's log. It does not stop the install.

## Consequences

- If qgp exits or loses the headset before the release, the override lapses by itself some minutes later.
- A future Horizon OS may drop the broadcast. The setting then does nothing, and the log says so only if the broadcast itself fails.
