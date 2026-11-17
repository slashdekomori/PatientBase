#!/usr/bin/env bash
# PatientBase build for RED OS 7.3 (Linux, x86_64).
#   ./build_redos7.sh                 build only
#   ./build_redos7.sh --install-deps  install build dependencies with dnf first
# Output: dist-redos7/ and PatientBase-redos7-x86_64.tar.gz
set -euo pipefail
cd "$(dirname "$0")"

. /etc/os-release 2>/dev/null || true
case "${VERSION_ID:-}" in
    7.3*) ;;
    *) echo "WARNING: this does not look like RED OS 7.3 (VERSION_ID='${VERSION_ID:-?}'). Continuing anyway." >&2 ;;
esac

if [ "${1:-}" = "--install-deps" ]; then
    # Package names are best guesses - verify with: dnf search qt6 | grep -E "base|postgres"
    COMMON="gcc-c++ make git"
    if ! sudo dnf install -y $COMMON qt6-qtbase-devel qt6-qtbase-postgresql; then
        echo "Qt 6 packages not available, trying Qt 5..." >&2
        sudo dnf install -y $COMMON qt5-qtbase-devel qt5-qtbase-postgresql
    fi
fi

# RED OS 7.3: prefer Qt 6, allow Qt 5 if that is all the repo has
export TARGET=redos7
export QMAKE_CANDIDATES="qmake6 qmake-qt6 qmake-qt5 qmake"
exec bash ./tools/build_common.sh
