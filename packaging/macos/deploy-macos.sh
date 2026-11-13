#!/bin/sh
# Makes a self-contained copy of the built app: QGPUI.app with Qt inside it. It does not sign the
# copy; sign-macos.sh does that. The qgpui-bundle target (build-bundle.sh) and the release
# workflow run it (decision 0022).
#
#   sh packaging/macos/deploy-macos.sh <macdeployqt> <built qgpui.app> <output app> [catalog]
#
# A catalog, when given, goes to Contents/Resources/qgpui-catalog.json, where the app looks for
# the catalog it shipped with. Contents/MacOS may hold only code, or the signature fails.

set -eu

[ $# -ge 3 ] && [ $# -le 4 ] ||
    { echo "usage: sh deploy-macos.sh <macdeployqt> <built app> <output app> [catalog]" >&2; exit 2; }
MACDEPLOYQT=$1
BUILT=$2
APP=$3
CATALOG=${4:-}

rm -rf "$APP"
mkdir -p "$(dirname "$APP")"
ditto "$BUILT" "$APP"

# Copies Qt's frameworks and plugins into the bundle. It reports errors for the SQL drivers whose
# client libraries this Mac lacks; those drivers are removed next.
"$MACDEPLOYQT" "$APP" -verbose=1

# QGPUI reads only SQLite databases.
find "$APP/Contents/PlugIns/sqldrivers" -name '*.dylib' ! -name 'libqsqlite.dylib' -delete

if [ -n "$CATALOG" ]; then
    cp "$CATALOG" "$APP/Contents/Resources/qgpui-catalog.json"
fi

for file in Contents/MacOS/qgpui Contents/MacOS/platform-tools/adb Contents/Frameworks/QtCore.framework \
            Contents/PlugIns/platforms/libqcocoa.dylib Contents/PlugIns/sqldrivers/libqsqlite.dylib; do
    [ -e "$APP/$file" ] || { echo "The deployed app has no $file." >&2; exit 1; }
done
echo "Deployed $APP"
