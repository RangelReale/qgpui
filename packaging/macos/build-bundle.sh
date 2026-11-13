#!/bin/sh
# Makes QGPUI.app and qgpui.dmg from a build, for this Mac or to try the packaging: deploys, signs
# (ad hoc unless an identity is given) and packages. It does not notarize; the release workflow
# does (decision 0022). The qgpui-bundle target (task bundle) runs it.
#
#   sh packaging/macos/build-bundle.sh <macdeployqt> <built qgpui.app> <output folder> <identity> [catalog]

set -eu

[ $# -ge 4 ] && [ $# -le 5 ] ||
    { echo "usage: sh build-bundle.sh <macdeployqt> <built app> <output folder> <identity> [catalog]" >&2; exit 2; }
HERE=$(cd "$(dirname "$0")" && pwd)
OUT=$3

sh "$HERE/deploy-macos.sh" "$1" "$2" "$OUT/QGPUI.app" ${5:+"$5"}
sh "$HERE/sign-macos.sh" "$OUT/QGPUI.app" "$4"
sh "$HERE/package-macos.sh" "$OUT/QGPUI.app" "$OUT/qgpui.dmg"
