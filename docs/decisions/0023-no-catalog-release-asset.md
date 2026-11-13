# 0023: Releases do not publish the catalog as a file

- Status: accepted
- Date: 2026-11-12

## Context

Decision 0019 had the release offer `qgpui-catalog.json` as a separate file, next to the
installer. Decision 0022 added the macOS disk image. Both downloads already include the catalog
that the release built.

## Decision

- A release publishes only the Windows installer and the macOS disk image. It does not upload
  `qgpui-catalog.json` on its own.
- The release workflow still builds the catalog in its own job and puts it in both downloads. The
  catalog stays a workflow artifact of the run, kept for a week.

This replaces the part of decision 0019 that offered the catalog as a separate file. The rest of
0019 stands.

## Consequences

- A user gets a catalog only with the app, or by building one from source.
- Releases made before this decision keep the catalog file they already published.
