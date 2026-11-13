#!/bin/sh
# Makes the disk image from a signed QGPUI.app: the app and a link to /Applications, so that the
# user drags the one onto the other (decision 0022).
#
#   sh packaging/macos/package-macos.sh <app> <disk image>

set -eu

[ $# -eq 2 ] || { echo "usage: sh package-macos.sh <app> <disk image>" >&2; exit 2; }
APP=$1
DMG=$2

STAGE=$(mktemp -d)
MOUNT=$(mktemp -d)
trap 'rm -rf "$STAGE"; rmdir "$MOUNT" 2>/dev/null || true' EXIT
# ditto keeps the signature and the symbolic links inside Qt's frameworks.
ditto "$APP" "$STAGE/QGPUI.app"
ln -s /Applications "$STAGE/Applications"

mkdir -p "$(dirname "$DMG")"
# hdiutil sometimes fails with "Resource busy" on a CI runner, and works when it runs again.
attempt=1
until hdiutil create -ov -srcfolder "$STAGE" -volname QGPUI -fs HFS+ -format UDZO "$DMG"; do
    if [ "$attempt" -ge 5 ]; then
        echo "hdiutil failed $attempt times." >&2
        exit 1
    fi
    attempt=$((attempt + 1))
    echo "hdiutil failed; trying again in 15 seconds." >&2
    sleep 15
done

hdiutil attach -readonly -nobrowse -mountpoint "$MOUNT" "$DMG" > /dev/null
contents=$(ls "$MOUNT" | tr '\n' ' ')
codesign --verify --deep --strict "$MOUNT/QGPUI.app" || { hdiutil detach "$MOUNT" > /dev/null; exit 1; }
hdiutil detach "$MOUNT" > /dev/null
if [ "$contents" != "Applications QGPUI.app " ]; then
    echo "The disk image holds: $contents" >&2
    exit 1
fi
echo "Disk image: $DMG"
