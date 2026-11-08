#!/usr/bin/env bash
# Boot logo: 640x480, black, "MintOS" in MinUI's font, "RG353V" small below.
# Writes os/splash/splash.png (preview) and os/files/usr/local/share/rg353/splash.raw
# (BGRX, the console framebuffer's format; no-fbcon.service copies it to /dev/fb0).
set -euo pipefail
cd "$(dirname "$0")/../.."
FONT=device/fonts/BPreplayBold-unhinted.otf
NAME=${1:-MintOS}
nix-shell -p imagemagick --run "
  magick -size 640x480 xc:black -font $FONT \
    -fill white -pointsize 72 -gravity center -annotate +0-14 '$NAME' \
    -fill '#808080' -pointsize 24 -gravity center -annotate +0+46 'RG353V' \
    os/splash/splash.png
  magick os/splash/splash.png -depth 8 bgra:os/files/usr/local/share/rg353/splash.raw
"
ls -l os/files/usr/local/share/rg353/splash.raw
