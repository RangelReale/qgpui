# Changelog

All notable changes to QGPUI are documented in this file. The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and QGPUI follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html). A pull request does not edit this file. It adds a fragment to [changelog.d](changelog.d/README.md), and the release moves the fragments here. See [decision 0027](docs/decisions/0027-changelog-fragments.md).

## [Unreleased]

## [1.2.0] - 2026-11-13

### Added

- QGPUI finds the PC games a port needs in the Epic Games Launcher's library too. The Store filter, the game dialog's store links and the recipes know Epic.
- The game dialog has an Uninstall button when the game is on the headset. It asks first, then removes the game with its saves and the files copied into its folders.

### Changed

- An install no longer waits at its last "do this in the headset" step. That step's text shows in the result banner instead.

### Fixed

- When the adb server stops while QGPUI runs, QGPUI starts it again. Before, the status bar showed "Starting adb..." until QGPUI was restarted.

## [1.1.0] - 2026-11-12

### Added

- The game dialog names the store that sells the original game, just above Install, with a link to its store page.

### Changed

- A release no longer has the catalog as a download of its own. The installer and the disk image still include one.

### Fixed

- Details opens the installation guide at the top. Before, it opened scrolled to the end.

## [1.0.0] - 2026-11-12

### Added

- The first release: browse the QuestPorts games as covers, sort and filter them, find the files each port needs in Steam, GOG and the files folder, and install and update games on a Meta Quest over USB or Wi-Fi.
- A Windows installer and a signed and notarized macOS disk image, each with a catalog.

[Unreleased]: https://github.com/RangelReale/qgpui/compare/v1.2.0...HEAD
[1.2.0]: https://github.com/RangelReale/qgpui/compare/v1.1.0...v1.2.0
[1.1.0]: https://github.com/RangelReale/qgpui/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/RangelReale/qgpui/releases/tag/v1.0.0
