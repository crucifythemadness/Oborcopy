#!/usr/bin/env bash
# Builds the self-contained Linux package from the installed binaries.
# Runs inside the Ubuntu 24.04 build container (see Dockerfile.linux).
#   bundle-linux.sh <install-prefix> <source-dir> <output-dir>
set -euo pipefail

PREFIX="$1"   # contains bin/oborcopy, bin/oborcopy-cli, bin/oborcopy-service
SRC="$2"      # repository root (packaging/, resources/)
OUT="$3"

VERSION="$(grep -oP 'VERSION \K[0-9]+\.[0-9]+\.[0-9]+' "$SRC/CMakeLists.txt" | head -1)"
NAME="oborcopy-${VERSION}-linux-x86_64"
DEST="$OUT/$NAME"
QT_PLUGINS="/usr/lib/x86_64-linux-gnu/qt6/plugins"

# Libraries that must come from the target system (glibc, graphics drivers,
# X11/Wayland core, fonts, C++ runtime) - same idea as the AppImage excludelist.
# libOpenGL.so.0 (GLVND front end, needed by libQt6Gui) IS bundled: many desktops
# without other Qt apps lack it, and it only dispatches to the host GL drivers.
EXCLUDE='^(linux-vdso|ld-linux|libc\.so|libm\.so|libdl\.so|libpthread\.so|librt\.so|libresolv\.so|libutil\.so|libmvec\.so|libstdc\+\+\.so|libgcc_s\.so|libGL|libEGL|libGLX|libGLdispatch|libdrm\.so|libgbm\.so|libX11\.so|libX11-xcb\.so|libxcb\.so\.1|libwayland-|libfontconfig\.so|libfreetype\.so|libasound\.so)'

copy_layout() {
    rm -rf "$OUT"
    mkdir -p "$DEST"/{bin,lib,plugins,share,LICENSES}
    cp "$PREFIX/bin/oborcopy" "$PREFIX/bin/oborcopy-cli" "$PREFIX/bin/oborcopy-service" "$DEST/bin/"
    mkdir -p "$DEST/plugins/platforms"
    for p in libqxcb.so libqoffscreen.so libqminimal.so; do
        cp "$QT_PLUGINS/platforms/$p" "$DEST/plugins/platforms/"
    done
    for d in xcbglintegrations platforminputcontexts imageformats; do
        [ -d "$QT_PLUGINS/$d" ] && cp -r "$QT_PLUGINS/$d" "$DEST/plugins/"
    done
    printf '[Paths]\nPrefix = ..\nPlugins = plugins\n' > "$DEST/bin/qt.conf"
}

# Copies every shared library the binaries and plugins need (transitively, as
# resolved by ldd) into lib/, except the excluded system libraries.
collect_libs() {
    local objects
    objects="$(find "$DEST/bin" "$DEST/plugins" -type f \( -name '*.so' -o -perm -u+x \) ! -name '*.conf')"
    # shellcheck disable=SC2086
    ldd $objects 2>/dev/null | awk '/=> \//{print $1" "$3}' | sort -u |
    while read -r soname path; do
        if ! echo "$soname" | grep -Eq "$EXCLUDE" && [ ! -e "$DEST/lib/$soname" ]; then
            cp -L "$path" "$DEST/lib/$soname"
        fi
    done
}

set_rpaths() {
    for f in "$DEST"/bin/oborcopy "$DEST"/bin/oborcopy-cli "$DEST"/bin/oborcopy-service; do
        patchelf --set-rpath '$ORIGIN/../lib' "$f"
    done
    for f in "$DEST"/lib/*.so*; do
        patchelf --set-rpath '$ORIGIN' "$f"
    done
    for f in "$DEST"/plugins/*/*.so; do
        patchelf --set-rpath '$ORIGIN/../../lib' "$f"
    done
}

add_extras() {
    local p="$SRC/packaging/linux"
    install -m 755 "$p/oborcopy.sh" "$DEST/oborcopy"
    install -m 755 "$p/oborcopy-cli.sh" "$DEST/oborcopy-cli"
    install -m 755 "$p/install-desktop-entry.sh" "$DEST/install-desktop-entry.sh"
    cp "$SRC/resources/oborcopy.png" "$DEST/share/"
    sed "s/@VERSION@/$VERSION/g" "$SRC/packaging/README-linux.txt" > "$DEST/README.txt"
    cp "$SRC"/packaging/LICENSES/* "$DEST/LICENSES/"
    cp "$SRC/docs/user-guide/Oborcopy-User-Guide.pdf" "$DEST/"
    sed -i 's/\r$//' "$DEST/oborcopy" "$DEST/oborcopy-cli" "$DEST/install-desktop-entry.sh" "$DEST/README.txt"
}

copy_layout
collect_libs
set_rpaths
add_extras
(cd "$OUT" && tar czf "$NAME.tar.gz" "$NAME")
echo "Bundled $(ls "$DEST/lib" | wc -l) libraries into $DEST"
