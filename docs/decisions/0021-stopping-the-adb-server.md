# 0021: QGPUI stops the adb server it started

- Status: accepted
- Date: 2026-11-12

## Context

When no adb server runs, QGPUI starts one with the adb it ships. adb then detaches and keeps running after QGPUI quits, so that later adb commands and other tools can use it.

On macOS this adb runs from inside `QGPUI.app`, and macOS counts it as part of the app. After QGPUI quits, macOS shows "QGPUI.app is running in the background" and lists it among the background apps. On Windows the leftover `adb.exe` in QGPUI's folder can stop an update or an uninstall from replacing it.

## Decision

On exit, after the keep-awake releases (decision 0015), QGPUI stops the adb server, but only if QGPUI started it. `HeadsetConnection::stopServerIfStarted` stops following the devices, so that the server is not started again, sends `host:kill` and waits up to two seconds for the answer.

A server that was already running when QGPUI started is left alone, because another tool started it and may still use it.

## Consequences

- Nothing of QGPUI keeps running after it quits, when QGPUI started the server.
- A tool that began to use QGPUI's server while QGPUI ran, such as scrcpy, loses its connection when QGPUI quits. Its next adb command starts a new server.
- Headsets connected over Wi-Fi are disconnected with the server. QGPUI connects the remembered headset again on its next start.
- If QGPUI crashes, the server keeps running, as before.
