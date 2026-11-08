#!/usr/bin/env bash
# Cross-compile the launcher for the RG353V (aarch64, Debian trixie) against the device's SDL2.
# build/sysroot-debian holds libSDL2/libSDL2_ttf copied from the device (fetched over SSH if missing).
set -euo pipefail
cd "$(dirname "$0")"
HOST=${RG353V:-root@rg353v}
SR=build/sysroot-debian
mkdir -p "$SR"
if [ ! -e "$SR/libSDL2.so" ]; then
  scp -q "$HOST:/usr/lib/aarch64-linux-gnu/libSDL2-2.0.so.0" "$HOST:/usr/lib/aarch64-linux-gnu/libSDL2_ttf-2.0.so.0" "$SR/"
  ln -sf libSDL2-2.0.so.0 "$SR/libSDL2.so"
  ln -sf libSDL2_ttf-2.0.so.0 "$SR/libSDL2_ttf.so"
fi
nix-shell -p zig SDL2.dev SDL2_ttf pkg-config --run "
  zig cc -target aarch64-linux-gnu.2.38 -O2 -Wall \$(pkg-config --cflags sdl2 SDL2_ttf) \
    launcher/rglauncher.c -o build/rglauncher -L$SR -lSDL2 -lSDL2_ttf \
    -Wl,--allow-shlib-undefined"
echo "built build/rglauncher"
