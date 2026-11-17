#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

REPO_ROOT="$(git rev-parse --show-toplevel)"
BUILD_LINUX="${REPO_ROOT}/build-linux"
DIST_LINUX="${REPO_ROOT}/dist-linux"
CLIENT="${REPO_ROOT}/client"

if ! command -v qmake6 >/dev/null; then
    echo "qmake6 not found. Install Qt 6 development packages first." >&2
    exit 1
fi

mkdir -p ${BUILD_LINUX}
(
    cd ${BUILD_LINUX}
    qmake6 ${REPO_ROOT}/client/PatientBase.pro CONFIG+=release
    make -j"$(nproc)"
)

rm -rf "$DIST_LINUX"
mkdir -p "$DIST_LINUX/Output/ALL_HYSTORY"
cp ${BUILD_LINUX}/PatientBase "$DIST_LINUX/"
cp ${CLIENT}/DocParser.bin ${CLIENT}/settings.ini "$DIST_LINUX/"
cp -r ${CLIENT}/Sources_files ${CLIENT}/шаблоны_основных_документов "$DIST_LINUX/"
chmod +x "$DIST_LINUX/DocParser.bin"

echo
echo "Built: $DIST_LINUX/PatientBase"
echo "Edit $DIST_LINUX/settings.ini (host/user/password), then run:"
echo "  cd $DIST_LINUX && ./PatientBase"
