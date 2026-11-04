#!/bin/bash
set -euo pipefail

VERSION="${NETO_VERSION:-0.3.0}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="$ROOT/dist"
STAGE="$ROOT/build/pkgstage"

VST3="$(find "$ROOT/build" -type d -name 'Neto Vocal.vst3' | head -1)"
AU="$(find "$ROOT/build" -type d -name 'Neto Vocal.component' | head -1)"
if [ -z "$VST3" ] || [ -z "$AU" ]; then
    echo "Plugin nao encontrado em $ROOT/build. Compile antes."
    exit 1
fi

rm -rf "$STAGE"
mkdir -p "$STAGE/vst3" "$STAGE/au" "$OUT"
cp -R "$VST3" "$STAGE/vst3/"
cp -R "$AU" "$STAGE/au/"

SIGN_ID="${APPLE_SIGN_ID:--}"
codesign --force --deep --sign "$SIGN_ID" "$STAGE/vst3/Neto Vocal.vst3"
codesign --force --deep --sign "$SIGN_ID" "$STAGE/au/Neto Vocal.component"

pkgbuild --root "$STAGE/vst3" --identifier com.netostudio.netovocal.vst3 --version "$VERSION" \
         --install-location "/Library/Audio/Plug-Ins/VST3" "$STAGE/vst3.pkg"
pkgbuild --root "$STAGE/au" --identifier com.netostudio.netovocal.au --version "$VERSION" \
         --install-location "/Library/Audio/Plug-Ins/Components" "$STAGE/au.pkg"

cat > "$STAGE/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
    <title>Neto Vocal $VERSION</title>
    <options customize="allow" require-scripts="false" hostArchitectures="arm64,x86_64"/>
    <choices-outline>
        <line choice="vst3"/>
        <line choice="au"/>
    </choices-outline>
    <choice id="vst3" title="VST3" description="Para Reaper, Cubase, Studio One, Ableton Live, FL Studio e outros.">
        <pkg-ref id="com.netostudio.netovocal.vst3"/>
    </choice>
    <choice id="au" title="Audio Unit" description="Para Logic Pro e GarageBand.">
        <pkg-ref id="com.netostudio.netovocal.au"/>
    </choice>
    <pkg-ref id="com.netostudio.netovocal.vst3">vst3.pkg</pkg-ref>
    <pkg-ref id="com.netostudio.netovocal.au">au.pkg</pkg-ref>
</installer-gui-script>
EOF

productbuild --distribution "$STAGE/distribution.xml" --package-path "$STAGE" \
             "$OUT/NetoVocal-$VERSION-macOS.pkg"

echo "Pronto: $OUT/NetoVocal-$VERSION-macOS.pkg"