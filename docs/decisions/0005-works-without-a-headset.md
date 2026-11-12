# 0005: qgp works without a headset

- Status: accepted
- Date: 2026-11-12

## Context

Much of qgp is useful with no Quest connected: browsing the catalog, reading a game's requirements, finding the PC files a game needs. adb itself may also be missing or its server stopped.

## Decision

No headset, and no adb, is a normal state, never a fatal one. Every device query (`qadb` packages, storage, properties, shell; update checks; installs) fails as an ordinary `Reply` that the app shows as "headset not connected" or similar. Device data is never a precondition for showing the catalog.

## Consequences

- Each headset-dependent feature needs an explicit "no device" state.
- Tests cover it: `tst_qadb_client::deviceQueriesFailSoftlyWithoutADevice` runs the device queries against an unknown device and against no server.
