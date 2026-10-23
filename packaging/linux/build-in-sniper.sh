#!/usr/bin/env bash
# Builds TF2PL inside the Steam Runtime 3 "sniper" SDK container (glibc 2.31) and packages the AppImage,
# so the result runs on every distro that can run 64-bit TF2 (TF2 itself requires sniper).
#
# This only uses the SDK as a *build* environment. TF2PL still runs on the host, never inside
# pressure-vessel/sniper: TF2PL launches TF2 through sniper itself, and nesting it would break that.
#
# Requires podman or docker. The source tree is mounted read-write; output goes to build-sniper/ and dist/.
#
# Usage: packaging/linux/build-in-sniper.sh [Release|Debug] [extra cmake args...]
#
# Discord: vcpkg doesn't provide the Discord Game SDK for static Linux builds, so it is off by default here.
# To include it, unzip https://dl-game-sdk.discordapp.net/3.2.1/discord_game_sdk.zip somewhere and run with
#   DISCORD_SDK_DIR=/path/to/discord_game_sdk packaging/linux/build-in-sniper.sh
# (the AppImage step then needs patchelf in the container; it is installed with pip if missing).
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_TYPE="${1:-Release}"
shift || true
IMAGE="${SNIPER_SDK_IMAGE:-registry.gitlab.steamos.cloud/steamrt/sniper/sdk:latest}"

if command -v podman >/dev/null 2>&1; then
  RUNTIME=podman
  USER_ARGS=(--userns=keep-id)
elif command -v docker >/dev/null 2>&1; then
  RUNTIME=docker
  USER_ARGS=(--user "$(id -u):$(id -g)")
else
  echo "podman or docker is required"; exit 1
fi

echo ">> using $RUNTIME with $IMAGE"
"$RUNTIME" pull "$IMAGE"

# vcpkg's binary cache lives in the repo so repeat builds are fast
mkdir -p "$REPO_ROOT/build-sniper" "$REPO_ROOT/.vcpkg-cache-sniper"

DISCORD_ARGS=(-e TF2PL_DISCORD=0)
if [ -n "${DISCORD_SDK_DIR:-}" ]; then
  [ -f "$DISCORD_SDK_DIR/lib/x86_64/discord_game_sdk.so" ] || { echo "no lib/x86_64/discord_game_sdk.so in $DISCORD_SDK_DIR"; exit 1; }
  DISCORD_ARGS=(-e TF2PL_DISCORD=1 -v "$(cd "$DISCORD_SDK_DIR" && pwd):/discord:ro")
fi

"$RUNTIME" run --rm "${USER_ARGS[@]}" \
  -v "$REPO_ROOT:/src" -w /src \
  -e HOME=/tmp \
  -e VCPKG_DEFAULT_BINARY_CACHE=/src/.vcpkg-cache-sniper \
  -e VCPKG_FORCE_SYSTEM_BINARIES=1 \
  "${DISCORD_ARGS[@]}" \
  "$IMAGE" bash -euo pipefail -c '
    # sniper ships several compilers; C++20 coroutines need gcc 12 (or a recent clang)
    if command -v g++-12 >/dev/null; then export CC=gcc-12 CXX=g++-12; fi
    echo ">> $($CXX --version | head -1), glibc $(ldd --version | head -1 | awk "{print \$NF}")"
    [ -x submodules/vcpkg/vcpkg ] || ./submodules/vcpkg/bootstrap-vcpkg.sh -disableMetrics
    DISCORD_CMAKE="-DTF2BD_ENABLE_DISCORD_INTEGRATION=OFF"
    if [ "$TF2PL_DISCORD" = 1 ]; then
      # same layout the Linux CI uses: lib as libdiscord_game_sdk.so, C headers under discord-game-sdk/, C++ wrapper sources
      mkdir -p /tmp/discord/lib /tmp/discord/include/discord-game-sdk /tmp/discord/src
      cp /discord/lib/x86_64/discord_game_sdk.so /tmp/discord/lib/libdiscord_game_sdk.so
      cp -r /discord/c/. /tmp/discord/include/discord-game-sdk/
      cp -r /discord/cpp /tmp/discord/src/discord-game-sdk
      DISCORD_CMAKE="-DTF2BD_ENABLE_DISCORD_INTEGRATION=ON -DCMAKE_LIBRARY_PATH=/tmp/discord/lib -DCMAKE_INCLUDE_PATH=/tmp/discord/include -DDISCORD_GAME_SDK_INCLUDE=/tmp/discord/src"
      command -v patchelf >/dev/null || { python3 -m pip install --user patchelf && export PATH="$HOME/.local/bin:$PATH"; }
    fi
    cmake -S . -B build-sniper -G Ninja $DISCORD_CMAKE \
      -DCMAKE_BUILD_TYPE='"$BUILD_TYPE"' \
      -DCMAKE_TOOLCHAIN_FILE=/src/submodules/vcpkg/scripts/buildsystems/vcpkg.cmake \
      '"$*"'
    cmake --build build-sniper
    cmake --build build-sniper --target appimage
  '

echo ">> done: $REPO_ROOT/dist/TF2PL-x86_64.AppImage (built against glibc 2.31)"
