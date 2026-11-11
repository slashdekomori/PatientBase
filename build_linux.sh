#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

if ! command -v qmake6 >/dev/null; then
    echo "qmake6 not found. Install Qt 6 development packages first." >&2
    exit 1
fi

mkdir -p build-linux
(
    cd build-linux
    qmake6 ../PatientBase.pro CONFIG+=release
    make -j"$(nproc)"
)

DIST="dist-linux"
rm -rf "$DIST"
mkdir -p "$DIST/Output/ALL_HYSTORY"
cp build-linux/PatientBase "$DIST/"
cp DocParser.bin settings.ini "$DIST/"
cp -r Sources_files шаблоны_основных_документов "$DIST/"
chmod +x "$DIST/DocParser.bin"

echo
echo "Built: $DIST/PatientBase"
echo "Edit $DIST/settings.ini (host/user/password), then run:"
echo "  cd $DIST && ./PatientBase"
