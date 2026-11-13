#!/bin/sh
# Signs a deployed QGPUI.app from the inside out (decision 0022).
#
#   sh packaging/macos/sign-macos.sh <app> <identity>
#
# With the identity "-", it signs ad hoc, which only the Mac that made the app trusts; adb keeps
# Google's own signature. With a Developer ID, it signs every piece, adb included, with the
# hardened runtime and a secure timestamp, as notarization requires. The app needs no
# entitlements.

set -eu

[ $# -eq 2 ] || { echo "usage: sh sign-macos.sh <app> <identity>" >&2; exit 2; }
APP=$1
IDENTITY=$2

if [ "$IDENTITY" = "-" ]; then
    set -- --force --sign -
else
    set -- --force --options runtime --timestamp --sign "$IDENTITY"
fi

# Loose libraries and plugins first, then each framework, after what it holds.
find "$APP/Contents/PlugIns" "$APP/Contents/Frameworks" -type f -name '*.dylib' -exec codesign "$@" {} +
for framework in "$APP"/Contents/Frameworks/*.framework; do
    [ -d "$framework" ] && codesign "$@" "$framework"
done
if [ "$IDENTITY" != "-" ]; then
    codesign "$@" "$APP/Contents/MacOS/platform-tools/adb"
fi
# The app last: it seals everything inside it.
codesign "$@" "$APP"

codesign --verify --deep --strict --verbose=2 "$APP"
