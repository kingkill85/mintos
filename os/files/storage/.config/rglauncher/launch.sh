#!/bin/bash
# launch.sh <rom> <system>: start a game with RetroArch (or ScummVM) on the rg353v OS.
ROM="$1"; SYS="$2"
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
    echo "$(date +%T) scummvm $ROM"
    # the .scummvm file holds the command line, e.g. --path="/storage/roms/scummvm/x" monkey
    eval "set -- $(cat "$ROM")"
    exec scummvm --fullscreen "$@" ;;
  *) echo "no emulator for $SYS"; exit 1 ;;
esac
echo "$(date +%T) $SYS core=$CORE rom=$ROM"
exec retroarch --verbose --config "$CFG" -L "$CORES/${CORE}_libretro.so" "$ROM"
