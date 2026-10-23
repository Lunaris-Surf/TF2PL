#!/usr/bin/env bash
# Builds the TF2PL AppImage from an already-built binary.
#
# Model: the AppImage is JUST the binary (+ its private shared libs). The runtime
# data/resource folders (cfg/ fonts/ images/ licenses/ tf2_addons/) are NOT packaged
# inside it — they ship next to the .AppImage file, exactly like the Windows portable
# zip. At runtime GetCurrentExeDir() resolves to dirname($APPIMAGE) (see
# Platform/Linux/Platform.cpp), so the binary finds them next to the file.
#
# Self-contained: downloads appimagetool if missing, uses the committed PNG icon
# (no Pillow/ImageMagick needed). Output: dist/TF2PL-x86_64.AppImage
#
# Usage: packaging/linux/build-appimage.sh [--with-resources] [path-to-binary]
#   --with-resources  also copy staging/{cfg,fonts,images,licenses,schemas,tf2_addons} next to the AppImage in dist/,
#                     so dist/ is a ready-to-zip portable folder
# Also available as a CMake target on Linux: cmake --build <build dir> --target appimage
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TOOLS="$REPO_ROOT/packaging/linux/tools"
WITH_RESOURCES=0
BIN_ARG=""
for arg in "$@"; do
  case "$arg" in
    --with-resources) WITH_RESOURCES=1 ;;
    -h|--help) sed -n '2,20p' "$0"; exit 0 ;;
    *) BIN_ARG="$arg" ;;
  esac
done
BIN="${BIN_ARG:-$REPO_ROOT/build/tf2_bot_detector/TF2PL}"
ICON="$REPO_ROOT/packaging/linux/TF2PL.png"
APPDIR="$REPO_ROOT/packaging/linux/AppDir"
OUTDIR="$REPO_ROOT/dist"
APPIMAGETOOL_URL="https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage"

export APPIMAGE_EXTRACT_AND_RUN=1   # FUSE-less environments (CI, sandboxes)

[ -f "$BIN" ]  || { echo "binary not found: $BIN (build the project first)"; exit 1; }
[ -f "$ICON" ] || { echo "icon not found: $ICON"; exit 1; }

echo ">> fetching appimagetool (if missing)"
mkdir -p "$TOOLS"
APPIMAGETOOL="$TOOLS/appimagetool.AppImage"
[ -f "$APPIMAGETOOL" ] || curl -fsSL -o "$APPIMAGETOOL" "$APPIMAGETOOL_URL"
chmod +x "$APPIMAGETOOL"

echo ">> assembling AppDir"
rm -rf "$APPDIR"; mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/lib" "$OUTDIR"
cp "$BIN" "$APPDIR/usr/bin/TF2PL"
cp "$ICON" "$APPDIR/TF2PL.png"
cp "$ICON" "$APPDIR/.DirIcon"

# Bundle private, non-system shared libs (everything else is statically linked).
# Leave the core system/loader and host graphics libs to the host.
echo ">> bundling shared libs"
ldd "$BIN" | awk '/=> \//{print $3}' | while read -r lib; do
  case "$(basename "$lib")" in
    libc.so.*|libm.so.*|libdl.so.*|libpthread.so.*|librt.so.*|ld-linux*) ;;  # host glibc
    libGL*|libEGL*|libX11*|libxcb*|libwayland*|libdrm*) ;;                    # host graphics
    *) cp -vL "$lib" "$APPDIR/usr/lib/" ;;
  esac
done

# Discord: the Game SDK ships discord_game_sdk.so without a SONAME, so a binary linked against it by full path
# records that absolute path (e.g. /usr/lib/libdiscord_game_sdk.so) and LD_LIBRARY_PATH can't redirect it.
# Rewrite such entries to plain names and give the binary an $ORIGIN rpath so the bundled copies are used.
BUNDLED_BIN="$APPDIR/usr/bin/TF2PL"
if command -v patchelf >/dev/null 2>&1; then
  echo ">> fixing absolute DT_NEEDED entries / rpath"
  for needed in $(patchelf --print-needed "$BUNDLED_BIN"); do
    case "$needed" in
      /*)
        base="$(basename "$needed")"
        [ -f "$APPDIR/usr/lib/$base" ] || cp -vL "$needed" "$APPDIR/usr/lib/$base"
        patchelf --replace-needed "$needed" "$base" "$BUNDLED_BIN"
        echo "   $needed -> $base"
        ;;
    esac
  done
  patchelf --set-rpath '$ORIGIN/../lib' "$BUNDLED_BIN"
else
  if readelf -d "$BUNDLED_BIN" 2>/dev/null | grep -q 'NEEDED.*\[/'; then
    echo "!! the binary needs a library by absolute path (usually the Discord SDK) and patchelf is not installed."
    echo "!! install patchelf (apt install patchelf) and re-run, or the AppImage won't start on other machines."
    exit 1
  fi
fi

if ldd "$BIN" | grep -q discord_game_sdk; then
  if ls "$APPDIR/usr/lib/" | grep -q discord_game_sdk; then
    echo ">> Discord Game SDK bundled"
  else
    echo "!! binary links the Discord Game SDK but it was not bundled"; exit 1
  fi
else
  echo ">> built without Discord integration (no Discord Game SDK to bundle)"
fi

echo ">> checking every library resolves from the AppDir"
if LD_LIBRARY_PATH="$APPDIR/usr/lib" ldd "$BUNDLED_BIN" | grep "not found"; then
  echo "!! unresolved libraries above"; exit 1
fi

echo ">> .desktop"
cat > "$APPDIR/TF2PL.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=TF2PL by LunarisV
Exec=TF2PL
Icon=TF2PL
Categories=Game;Utility;
Terminal=false
EOF

echo ">> AppRun"
cat > "$APPDIR/AppRun" <<'EOF'
#!/bin/sh
HERE="$(dirname "$(readlink -f "$0")")"
export LD_LIBRARY_PATH="$HERE/usr/lib:$LD_LIBRARY_PATH"
# wayland SDL backend crashes (see staging/launch_tf2bd_linux.sh); default to x11.
export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-x11}"
# Do NOT chdir: the binary uses $APPIMAGE to find its data folder next to the file.
exec "$HERE/usr/bin/TF2PL" "$@"
EOF
chmod +x "$APPDIR/AppRun"

echo ">> appimagetool"
"$APPIMAGETOOL" "$APPDIR" "$OUTDIR/TF2PL-x86_64.AppImage"

if [ "$WITH_RESOURCES" = 1 ]; then
  echo ">> copying portable resources next to the AppImage"
  for dir in cfg fonts images licenses schemas tf2_addons; do
    [ -d "$REPO_ROOT/staging/$dir" ] && cp -r "$REPO_ROOT/staging/$dir" "$OUTDIR/"
  done
fi
echo ">> done: $OUTDIR/TF2PL-x86_64.AppImage"
