#!/usr/bin/env bash
# Slim frames for TV consoles around rgarch's "Ganzzahlig" picture (4:3, height 2x):
#   SNES/Mega Drive 256|320x224 -> 597x448 at 21,16    Master System 256x192 -> 512x384 at 64,48
# The frame never changes the picture; it only fills the black border.
#   os/frames/make-frames.sh  -> os/files/storage/.config/retroarch/overlays/tv/*.png
set -euo pipefail
cd "$(dirname "$0")"
OUT=../files/storage/.config/retroarch/overlays/tv
mkdir -p "$OUT"
# frame <out> <x> <y> <w> <h> <base> <edge> <accent|none> <logo: sfc|none>
frame() {
  local out=$1 x=$2 y=$3 w=$4 h=$5 base=$6 edge=$7 accent=$8 logo=$9
  local x1=$((x + w - 1)) y1=$((y + h - 1))
  local args=(-size 640x480 "xc:$base"
    # soft light from the top, darker at the bottom
    \( -size 640x480 gradient:'rgba(255,255,255,0.10)-rgba(0,0,0,0.18)' \) -compose over -composite
    # bevel: an edge line around the window
    -fill none -stroke "$edge" -strokewidth 3 -draw "roundrectangle $((x-2)),$((y-2)) $((x1+2)),$((y1+2)) 7,7")
  [ "$accent" != none ] && args+=(-stroke "$accent" -strokewidth 2 -draw "roundrectangle $((x-6)),$((y-6)) $((x1+6)),$((y1+6)) 9,9")
  if [ "$logo" = sfc ]; then   # the four button colours, small, centred in the left border
    local cx=$((x / 2)) cy=240 r=3 d=5
    args+=(-stroke none
      -fill '#e4202b' -draw "circle $((cx+d)),$cy $((cx+d+r)),$cy"
      -fill '#f6c400' -draw "circle $cx,$((cy+d)) $((cx+r)),$((cy+d))"
      -fill '#1e9a46' -draw "circle $((cx-d)),$cy $((cx-d+r)),$cy"
      -fill '#2b6bd6' -draw "circle $cx,$((cy-d)) $((cx+r)),$((cy-d))")
  fi
  # cut the window (exactly the picture) transparent
  magick "${args[@]}" \( -size 640x480 xc:white -fill black -draw "rectangle $x,$y $x1,$y1" \) \
    -alpha off -compose copy-opacity -composite "$OUT/$out"
}
S="21 16 597 448"; M="64 48 512 384"
frame snes-schwarz.png $S '#171717' '#3c3c3c' none sfc
frame snes-grau.png    $S '#c6c5cb' '#8f8d99' none sfc
frame snes-lila.png    $S '#c6c5cb' '#8f8d99' '#5b4a8b' none
frame sfc-hell.png     $S '#d9d8dc' '#a9a7b1' none sfc
frame md-schwarz.png   $S '#141414' '#3a3a3a' '#b81d24' none
frame md-grau.png      $S '#2a2a2c' '#555559' none none
frame sms-schwarz.png  $M '#141414' '#3a3a3a' '#b81d24' none
frame sms-grau.png     $M '#2a2a2c' '#555559' none none
echo "frames in $OUT"
