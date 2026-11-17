#!/usr/bin/env bash
# Shared build+bundle logic. Do not run directly: use ../build_redos7.sh or ../build_redos8.sh.
# Env: TARGET (output name, e.g. redos7), QMAKE_CANDIDATES (space-separated, in order of preference).
# Produces build-$TARGET/, dist-$TARGET/ and PatientBase-$TARGET-<arch>.tar.gz with Qt libs
# and the QPSQL plugin bundled, so Qt need not be installed on the target machine.
set -euo pipefail
cd "$(dirname "$0")"

REPO_ROOT="$(git rev-parse --show-toplevel)"
TARGET="${TARGET:-linux}"
BUILD_LINUX="${REPO_ROOT}/build-${TARGET}"
DIST_LINUX="${REPO_ROOT}/dist-${TARGET}"
CLIENT="${REPO_ROOT}/client"

# --- find qmake (Qt 6 preferred, Qt 5 as fallback) ---------------------------
QMAKE=""
for c in ${QMAKE_CANDIDATES:-qmake6 qmake-qt6 qmake-qt5 qmake}; do
    if command -v "$c" >/dev/null; then QMAKE="$c"; break; fi
done
if [ -z "$QMAKE" ]; then
    echo "qmake not found (tried: ${QMAKE_CANDIDATES:-qmake6 qmake-qt6 qmake-qt5 qmake}). Install Qt development packages first." >&2
    exit 1
fi
echo "Using $QMAKE ($("$QMAKE" -query QT_VERSION))"

# --- build -------------------------------------------------------------------
rm -rf "$BUILD_LINUX"
mkdir -p "$BUILD_LINUX"
(
    cd "$BUILD_LINUX"
    "$QMAKE" "${CLIENT}/PatientBase.pro" CONFIG+=release
    make -j"$(nproc)"
)

# --- assemble dist -----------------------------------------------------------
rm -rf "$DIST_LINUX"
mkdir -p "$DIST_LINUX/Output/ALL_HYSTORY" "$DIST_LINUX/lib" "$DIST_LINUX/plugins"
cp "${BUILD_LINUX}/PatientBase" "$DIST_LINUX/"
cp "${CLIENT}/DocParser.bin" "${CLIENT}/settings.ini" "$DIST_LINUX/"
cp -r "${CLIENT}/Sources_files" "${CLIENT}/шаблоны_основных_документов" "$DIST_LINUX/"
chmod +x "$DIST_LINUX/DocParser.bin"

# --- bundle Qt plugins -------------------------------------------------------
QT_PLUGINS="$("$QMAKE" -query QT_INSTALL_PLUGINS)"
for p in platforms imageformats iconengines platformthemes xcbglintegrations sqldrivers; do
    [ -d "$QT_PLUGINS/$p" ] && cp -r "$QT_PLUGINS/$p" "$DIST_LINUX/plugins/"
done

if [ ! -f "$DIST_LINUX/plugins/sqldrivers/libqsqlpsql.so" ]; then
    echo "ERROR: libqsqlpsql.so (Qt PostgreSQL driver) not found in $QT_PLUGINS/sqldrivers." >&2
    echo "Install it (e.g. qt6-qtbase-postgresql) and re-run." >&2
    exit 1
fi
# keep only the postgres driver
find "$DIST_LINUX/plugins/sqldrivers" -type f ! -name 'libqsqlpsql.so' -delete

# tell Qt where the bundled plugins are (path is relative to the executable)
printf '[Paths]\nPlugins=plugins\n' > "$DIST_LINUX/qt.conf"

# --- bundle shared libraries (everything ldd resolves, minus core system libs) ---
# These must come from the target OS itself, so they are excluded.
EXCLUDE='linux-vdso|ld-linux|/libc\.so|/libm\.so|/libdl\.so|/libpthread\.so|/librt\.so|/libutil\.so|/libresolv\.so|/libGL\.|/libEGL\.|/libGLX|/libOpenGL|/libdrm|/libgbm|/libX11\.so|/libxcb\.so|/libfontconfig|/libfreetype|/libstdc\+\+|/libgcc_s'
while read -r f; do
    ldd "$f" 2>/dev/null | awk '/=> \//{print $3}' | grep -Ev "$EXCLUDE" || true
done < <(echo "$DIST_LINUX/PatientBase"; find "$DIST_LINUX/plugins" -name '*.so') \
  | sort -u | while read -r lib; do cp -n "$lib" "$DIST_LINUX/lib/"; done

# --- launcher ----------------------------------------------------------------
cat > "$DIST_LINUX/run.sh" <<'RUN'
#!/usr/bin/env bash
cd "$(dirname "$(readlink -f "$0")")"
export LD_LIBRARY_PATH="$PWD/lib:${LD_LIBRARY_PATH:-}"
exec ./PatientBase "$@"
RUN
chmod +x "$DIST_LINUX/run.sh"

# --- icon + one-time installer (menu entry and desktop icon) -----------------
if [ -f "${CLIENT}/icon.png" ]; then
    cp "${CLIENT}/icon.png" "$DIST_LINUX/icon.png"
else
    echo "WARNING: client/icon.png not found - the shortcut will have no icon." >&2
fi
cp "${REPO_ROOT}/tools/install.sh" "$DIST_LINUX/install.sh"
chmod +x "$DIST_LINUX/install.sh"

# --- sanity checks -----------------------------------------------------------
echo
echo "Missing libraries (should be empty):"
LD_LIBRARY_PATH="$DIST_LINUX/lib" ldd "$DIST_LINUX/PatientBase" | grep "not found" || echo "  none"
LD_LIBRARY_PATH="$DIST_LINUX/lib" ldd "$DIST_LINUX/plugins/sqldrivers/libqsqlpsql.so" | grep "not found" || true
ls "$DIST_LINUX/lib" | grep -q '^libpq\.so' || echo "WARNING: libpq.so not bundled - check ldd output of libqsqlpsql.so"

# --- archive -----------------------------------------------------------------
ARCHIVE="${REPO_ROOT}/PatientBase-${TARGET}-$(uname -m).tar.gz"
tar -czf "$ARCHIVE" -C "$REPO_ROOT" --transform "s,^dist-${TARGET},PatientBase," "dist-${TARGET}"

echo
echo "Built:   $DIST_LINUX"
echo "Archive: $ARCHIVE"
echo "Edit settings.ini (host/user/password), then run ./install.sh once to get the menu entry and desktop icon."
