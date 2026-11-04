#!/bin/bash
set -euo pipefail

VERSION="${NETO_VERSION:-0.3.0}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="$ROOT/dist"
STAGE="$ROOT/build/linuxstage"

VST3="$(find "$ROOT/build" -type d -name 'Neto Vocal.vst3' | head -1)"
if [ -z "$VST3" ]; then
    echo "Plugin nao encontrado em $ROOT/build. Compile antes."
    exit 1
fi

rm -rf "$STAGE"
mkdir -p "$STAGE/deb/DEBIAN" "$STAGE/deb/usr/lib/vst3" "$OUT"

cp -R "$VST3" "$STAGE/deb/usr/lib/vst3/"
cat > "$STAGE/deb/DEBIAN/control" <<EOF
Package: neto-vocal
Version: $VERSION
Section: sound
Priority: optional
Architecture: amd64
Maintainer: Neto Studio <neto@localhost>
Depends: libasound2t64 | libasound2, libfreetype6, libfontconfig1, libx11-6, libxext6, libxrandr2, libxinerama1, libxcursor1
Description: Neto Vocal - plugin VST3 de voz
 Glue vocal, presenca, corpo, claridade, espacialidade, 10 presets e Voice Match.
EOF
dpkg-deb --build --root-owner-group "$STAGE/deb" "$OUT/NetoVocal-$VERSION-Linux-amd64.deb"

PKG="$STAGE/tar/NetoVocal-$VERSION-Linux"
mkdir -p "$PKG"
cp -R "$VST3" "$PKG/"
cat > "$PKG/install.sh" <<'EOF'
#!/bin/bash
set -e
cd "$(dirname "$0")"
mkdir -p "$HOME/.vst3"
cp -R "Neto Vocal.vst3" "$HOME/.vst3/"
echo "Neto Vocal instalado em $HOME/.vst3. Abra o seu DAW e faca um novo scan de plugins."
EOF
chmod +x "$PKG/install.sh"
tar -czf "$OUT/NetoVocal-$VERSION-Linux.tar.gz" -C "$STAGE/tar" "NetoVocal-$VERSION-Linux"

echo "Pronto: $OUT"