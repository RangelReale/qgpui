# 0012: one qgp per user

- Status: accepted
- Date: 2026-11-12

## Context

Two qgp windows could run installs on the same headset at once and write the same install records and settings. A user who starts qgp again usually wants the window that is already open.

## Decision

Only one qgp runs per user. The first instance takes a lock file in the temporary folder (`QLockFile`) and listens on a local socket (`QLocalServer`). A later instance finds the lock taken, sends its catalog argument over the socket and quits. The first instance then opens that catalog, if one was given, and brings its window to the front.

The lock file decides which instance runs, not the socket, because on Windows two processes can listen on the same pipe name. A lock is stale only when its process is gone, so a crashed qgp never blocks the next start. When the lock file cannot be written at all, qgp runs anyway.

On Windows, the second instance calls `AllowSetForegroundWindow` before it sends, because Windows lets only the foreground process bring a window to the front.

## Consequences

- Two users on one PC each get their own qgp: the lock and socket names include a hash of the user name.
- `SingleInstance` in `apps/qgp` holds the code; `main.cpp` claims the instance before it creates the window.
