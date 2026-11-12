# 0004: Identify a headset by its hardware serial

- Status: accepted
- Date: 2026-11-12

## Context

qgp keeps per-headset state, such as what it installed. Over Wi-Fi the adb serial is `IP:port`: it changes with the network, and one headset can appear twice, on port 5555 and on its wireless-debugging port.

## Decision

qgp identifies a headset by `getprop ro.serialno`, the hardware serial that adb also shows over USB. `qadb::Client::getProperty()` reads it. Per-headset records are keyed by it.

## Consequences

- Records survive changes between USB and Wi-Fi, and new IP addresses.
- Two adb entries with the same `ro.serialno` are one headset, which the app can show once.
- Reading the serial takes one shell request after a device connects.
