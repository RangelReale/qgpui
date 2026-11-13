#!/bin/sh
# Makes a copy of qgpui.app that runs on other Macs, and a disk image of it. The qgpui-bundle
# CMake target runs this script; see decision 0020.
#
# Usage: make-bundle.sh <macdeployqt> <built qgpui.app> <output folder> <signing identity>
#
# The identity "-" signs ad hoc, which only this Mac trusts. A Developer ID certificate also
# turns on the hardened runtime and a secure timestamp, which Apple's notary service requires.

set -eu

macdeployqt=$1
built=$2
out=$3
identity=$4
app=$out/qgpui.app

rm -rf "$out"
mkdir -p "$out"
cp -R "$built" "$app"

# Copy Qt's frameworks and plugins into the bundle. Signing comes later, after the cleanup.
"$macdeployqt" "$app" -verbose=1

# QGPUI reads only SQLite databases. The other SQL drivers need client libraries that most Macs
# do not have.
find "$app/Contents/PlugIns/sqldrivers" -name '*.dylib' ! -name 'libqsqlite.dylib' -delete

if [ "$identity" = "-" ]; then
    set -- --force --sign -
else
    set -- --force --sign "$identity" --options runtime --timestamp
fi

# Sign from the inside out: plugins and frameworks, then the app, which seals the rest.
# platform-tools/adb keeps Google's own signature.
find "$app/Contents/PlugIns" "$app/Contents/Frameworks" -name '*.dylib' -exec codesign "$@" {} +
for framework in "$app"/Contents/Frameworks/*.framework; do
    codesign "$@" "$framework"
done
codesign "$@" "$app"
codesign --verify --deep --strict "$app"

hdiutil create -quiet -volname QGPUI -srcfolder "$app" -ov -format UDZO "$out/qgpui.dmg"
echo "Made $app and $out/qgpui.dmg"
