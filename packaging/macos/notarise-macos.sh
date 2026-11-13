#!/bin/sh
# Signs a disk image, notarizes it with an App Store Connect API key, and staples the ticket to
# it. The release workflow's signing job runs it (decision 0022).
#
#   sh packaging/macos/notarise-macos.sh <disk image> <identity>
#
# Needs QGPUI_NOTARY_KEY (the path of the .p8 file), QGPUI_NOTARY_KEY_ID and QGPUI_NOTARY_ISSUER.

set -eu

[ $# -eq 2 ] || { echo "usage: sh notarise-macos.sh <disk image> <identity>" >&2; exit 2; }
DMG=$1
IDENTITY=$2

notary() {
    xcrun notarytool "$@" --key "$QGPUI_NOTARY_KEY" --key-id "$QGPUI_NOTARY_KEY_ID" \
        --issuer "$QGPUI_NOTARY_ISSUER"
}

codesign --sign "$IDENTITY" --timestamp "$DMG"

# notarytool can exit with 0 for a rejected submission, so check the status it prints.
RESULT=$(notary submit "$DMG" --wait)
echo "$RESULT"
if ! echo "$RESULT" | grep -q "status: Accepted"; then
    ID=$(echo "$RESULT" | awk '/^ *id:/ { print $2; exit }')
    echo "Notarization failed. Apple's log:" >&2
    notary log "$ID" >&2 || true
    exit 1
fi
# Apple's ticket server sometimes drops the connection, so the staple is tried a few times.
attempt=1
until xcrun stapler staple "$DMG"; do
    if [ "$attempt" -ge 5 ]; then
        echo "Stapling failed $attempt times." >&2
        exit 1
    fi
    attempt=$((attempt + 1))
    echo "Stapling failed; trying again in 30 seconds." >&2
    sleep 30
done
spctl --assess --type open --context context:primary-signature --verbose "$DMG"
