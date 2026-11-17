#!/usr/bin/env bash
# PatientBase build for RED OS 8 (Linux, x86_64).
#   ./build_redos8.sh                 build only
#   ./build_redos8.sh --install-deps  install build dependencies with dnf first
# Output: dist-redos8/ and PatientBase-redos8-x86_64.tar.gz
set -euo pipefail
cd "$(dirname "$0")"

. /etc/os-release 2>/dev/null || true
case "${VERSION_ID:-}" in
    8*) ;;
    *) echo "WARNING: this does not look like RED OS 8 (VERSION_ID='${VERSION_ID:-?}'). Continuing anyway." >&2 ;;
esac

if [ "${1:-}" = "--install-deps" ]; then
    # Package names are best guesses - verify with: dnf search qt6 | grep -E "base|postgres"
    sudo dnf install -y gcc-c++ make git qt6-qtbase-devel qt6-qtbase-postgresql
fi

# RED OS 8: Qt 6 only (matches the Windows build)
export TARGET=redos8
export QMAKE_CANDIDATES="qmake6 qmake-qt6"
exec bash ./tools/build_common.sh
