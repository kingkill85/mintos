#!/usr/bin/env bash
# Cross-compile the launcher for the RG353V (aarch64, Debian trixie) against the device's SDL2.
# build/sysroot-debian holds libSDL2/libSDL2_ttf copied from the device (fetched over SSH if missing).
set -euo pipefail
cd "$(dirname "$0")"
HOST=${RG353V:-root@192.168.178.37}
SR=build/sysroot-debian
mkdir -p "$SR"
if [ ! -e "$SR/libSDL2.so" ]; then
  scp -q "$HOST:/usr/lib/aarch64-linux-gnu/libSDL2-2.0.so.0" "$HOST:/usr/lib/aarch64-linux-gnu/libSDL2_ttf-2.0.so.0" "$SR/"
  ln -sf libSDL2-2.0.so.0 "$SR/libSDL2.so"
  ln -sf libSDL2_ttf-2.0.so.0 "$SR/libSDL2_ttf.so"
fi
if [ ! -e "$SR/libz.so" ]; then
  scp -q "$HOST:/usr/lib/aarch64-linux-gnu/libz.so.1" "$SR/" && ln -sf libz.so.1 "$SR/libz.so"
fi
if [ ! -e "$SR/libsystemd.so" ]; then
  scp -q "$HOST:/usr/lib/aarch64-linux-gnu/libsystemd.so.0" "$SR/" && ln -sf libsystemd.so.0 "$SR/libsystemd.so"
fi
if [ ! -e "$SR/libSDL2_image.so" ]; then
  scp -q "$HOST:/usr/lib/aarch64-linux-gnu/libSDL2_image-2.0.so.0" "$SR/"
  ln -sf libSDL2_image-2.0.so.0 "$SR/libSDL2_image.so"
fi
VER=$(date +%Y-%m-%d)-$(git rev-parse --short HEAD)
CC="zig cc -target aarch64-linux-gnu.2.38 -O2 -Wall -DMINTOS_VERSION=\\\"$VER\\\""
nix-shell -p zig SDL2.dev SDL2_ttf SDL2_image zlib.dev systemd.dev pkg-config --run "
  set -e
  CF=\$(pkg-config --cflags sdl2 SDL2_ttf SDL2_image zlib)
  $CC \$CF -Icommon -Ifrontend launcher/rglauncher.c launcher/settings.c launcher/job.c launcher/keyboard.c common/ui.c common/settings.c common/osd.c frontend/config.c frontend/gameopts.c -o build/rglauncher -L$SR -lSDL2 -lSDL2_ttf -lm -Wl,--allow-shlib-undefined
  echo 'built build/rglauncher'
  $CC \$CF -Ifrontend -Icommon \$(ls frontend/*.c common/*.c 2>/dev/null) -o build/rgarch -L$SR -lSDL2 -lSDL2_ttf -lSDL2_image -lz -ldl -lm -Wl,--allow-shlib-undefined
  echo 'built build/rgarch'
  # system tools (C, replace the former Python helpers) -> build/tools/, installed to /usr/local/sbin
  T=build/tools; mkdir -p \$T/gen
  echo '#include <linux/input-event-codes.h>' | zig cc -target aarch64-linux-gnu -E -dM -x c - | sed -n 's/^#define \\(\\(KEY\\|BTN\\)_[A-Za-z0-9_]*\\) .*/{\"\\1\", \\1},/p' | sort > \$T/gen/keynames.h
  TC=\"$CC -Wextra -Itools\"
  \$TC tools/mintosd.c tools/mintosd_helpers.c tools/util.c -o \$T/mintosd -lpthread
  \$TC \$(pkg-config --cflags libsystemd) tools/mintos-wifi.c tools/wifi.c tools/util.c -o \$T/mintos-wifi -L$SR -lsystemd -Wl,--allow-shlib-undefined
  \$TC tools/mintos-bt.c tools/bt.c tools/util.c -o \$T/mintos-bt
  \$TC \$(pkg-config --cflags zlib) tools/fbgrab.c tools/fb.c -o \$T/fbgrab -L$SR -lz
  \$TC tools/mi2-copyprotect.c tools/fb.c tools/util.c -o \$T/mi2-copyprotect
  \$TC -I\$T/gen tools/rg353-press.c tools/util.c -o \$T/rg353-press
  ln -sf rg353-press \$T/rg353-type
  echo 'built build/tools:' \$(ls \$T | grep -v gen)
"
