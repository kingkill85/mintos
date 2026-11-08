#!/bin/bash
# launch.sh <rom> <system>: start a game with RetroArch (or ScummVM) on the rg353v OS.
ROM="$1"; SYS="$2"
# for rg353-power: which game is running (resumed after the power key switched off)
printf '%s\t%s\n' "$SYS" "$ROM" > /run/rglauncher-current
CORES=/storage/.config/retroarch/cores   # libretro buildbot builds (Debian's are years old)
CFG=/storage/.config/retroarch/retroarch.cfg
case "$SYS" in
  gb|gbc)                                    CORE=gambatte ;;
  gba)                                       CORE=mgba ;;
  nes)                                       CORE=nestopia ;;
  snes)                                      CORE=snes9x ;;
  mastersystem|gamegear|megadrive|genesis|megadrive-japan|sg-1000|segacd|megacd)
                                             CORE=genesis_plus_gx ;;
  sega32x)                                   CORE=picodrive ;;
  famicom|fds)                               CORE=nestopia ;;
  sfc)                                       CORE=snes9x ;;
  pcengine|tg16|pcenginecd|tg16cd)           CORE=mednafen_pce_fast ;;
  ngp|ngpc)                                  CORE=mednafen_ngp ;;
  wonderswan|wonderswancolor)                CORE=mednafen_wswan ;;
  atari2600)                                 CORE=stella2014 ;;
  atari7800)                                 CORE=prosystem ;;
  atarilynx)                                 CORE=handy ;;
  virtualboy)                                CORE=mednafen_vb ;;
  arcade|fbneo|neogeo|cps1|cps2|cps3)        CORE=fbneo ;;
  psx)                                       CORE=pcsx_rearmed ;;
  n64)                                       CORE=mupen64plus_next ;;
  nds)                                       CORE=melondsds ;;
  psp)                                       CORE=ppsspp ;;
  dreamcast)                                 CORE=flycast ;;
  scummvm)
    # The list holds ROCKNIX-style files: --path="<game dir>" <id>. RetroArch's ScummVM core
    # wants <game dir>/<id>.scummvm containing the id; it starts the game without ScummVM's GUI.
    eval "set -- $(cat "$ROM")"
    DIR=${1#--path=}; ID=$2
    [ -f "$DIR/$ID.scummvm" ] || echo "$ID" > "$DIR/$ID.scummvm"
    ROM="$DIR/$ID.scummvm"; CORE=scummvm
    # Monkey Island 2: ScummVM accepts any answer to the code wheel; type one automatically
    [ "$ID" = monkey2 ] && /usr/local/sbin/mi2-copyprotect >/dev/null 2>&1 &
    ;;
  *) echo "no emulator for $SYS"; exit 1 ;;
esac
echo "$(date +%T) $SYS core=$CORE rom=$ROM resume=${RESUME:-0}"
# RESUME=1 (set by rglauncher after a power-off in game): load the auto save state
EXTRA=; [ "$RESUME" = 1 ] && EXTRA="--appendconfig /storage/.config/retroarch/resume.cfg"
exec retroarch --verbose --config "$CFG" $EXTRA -L "$CORES/${CORE}_libretro.so" "$ROM"
