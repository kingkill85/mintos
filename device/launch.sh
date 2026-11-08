#!/bin/bash
# launch.sh <rom> <system>: start a game with ROCKNIX's default emulator/core for the system.
ROM="$1"; SYS="$2"
CFG=/storage/.config/emulationstation/es_systems.cfg
read -r EMU CORE < <(awk -v sys="$SYS" '
  /<system>/ { insys=0 }
  $0 ~ "<name>" sys "</name>" { insys=1 }
  insys && /<emulator name=/ { match($0, /name="[^"]+"/); emu=substr($0, RSTART+6, RLENGTH-7) }
  insys && /<core default="true">/ { gsub(/.*<core default="true">|<\/core>.*/, ""); print emu, $0; exit }
' "$CFG")
echo "$(date +%T) $SYS emu=$EMU core=$CORE rom=$ROM"
exec /usr/bin/runemu.sh "$ROM" -P"$SYS" --core="$CORE" --emulator="$EMU" --controllers=""
