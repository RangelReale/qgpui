#!/bin/sh
# Renders qgpui.svg into qgpui-<size>.png, the macOS icon from 16 to 1024 pixels. The build turns
# them into qgpui.icns, and on macOS the app also uses them as its window and Dock icon.
# Needs rsvg-convert (brew install librsvg). The outputs are committed; run this again only to
# change the icon.

set -eu
here=$(cd "$(dirname "$0")" && pwd)
for size in 16 32 64 128 256 512 1024; do
    rsvg-convert --width "$size" --height "$size" --output "$here/qgpui-$size.png" "$here/qgpui.svg"
done
echo "Wrote the macOS icon PNGs to $here"
