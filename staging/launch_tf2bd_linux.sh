#!/bin/sh
# SDL_VIDEODRIVER="x11" as wayland just... crashes lol
cd "$(dirname "$0")"
SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-x11}" LD_LIBRARY_PATH="$LD_LIBRARY_PATH:./" exec ./TF2PL "$@"
