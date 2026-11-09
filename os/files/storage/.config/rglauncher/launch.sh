#!/bin/bash
# launch.sh <rom> <system>: start a game. Systems with a section in rgarch's systems.cfg run
# in rgarch (core and settings from there); n64, nds, psp and dreamcast still in RetroArch.
# RESUME=1 (set by rglauncher after a power-off during a game) continues from the auto state.
# FRESH=1 (X in the game list) starts the game from the beginning instead of where it was left.
ROM="$1"; SYS="$2"; LIST="$1"
TITLE=$(basename "$ROM"); TITLE=$(printf "%s" "${TITLE%.*}" | sed "s/ *[([][^])]*[])]//g")   # as in the list
# for mintosd (power key): which game is running
printf '%s\t%s\n' "$SYS" "$ROM" > /run/rglauncher-current

# ScummVM: prepares ROM and scummvm.ini; again after a Neustart (rgarch exit code 3)
scumm_prep() {
  # The list holds ROCKNIX-style files: --path="<game dir>" <id>. The ScummVM core wants
  # <game dir>/<id>.scummvm containing the id; it starts the game without ScummVM's GUI.
  eval "set -- $(cat "$LIST")"
  DIR=${1#--path=}; ID=$2
  [ -f "$DIR/$ID.scummvm" ] || echo "$ID" > "$DIR/$ID.scummvm"
  ROM="$DIR/$ID.scummvm"
  # Continue where the game was left: rgarch quicksaves into slot 10 on quit; save_slot in the
  # game's section of scummvm.ini makes the engine load it at start (the core starts a target
  # that has a section from there; gameid + path suffice, ScummVM adds the rest itself).
  INI=/storage/roms/bios/scummvm.ini
  SLOT=; [ "$FRESH" != 1 ] && [ -f "/storage/saves/scummvm/$ID.s10" ] && SLOT=10
  case "$ID" in *:*) ;; *)
    touch "$INI"
    awk -v sec="[$ID]" -v gid="$ID" -v dir="$DIR" -v slot="$SLOT" '
      function close_sec() { if (!hasg) print "gameid=" gid; if (!hasp) print "path=" dir
                             if (slot != "") print "save_slot=" slot; in_sec = 0 }
      /^\[/ { if (in_sec) close_sec(); for (; blank > 0; blank--) print ""
              in_sec = ($0 == sec); if (in_sec) { found = 1; hasg = hasp = 0 }; print; next }
      /^[ \t]*$/ { blank++; next }
      { for (; blank > 0; blank--) print "" }
      in_sec && /^save_slot=/ { next }
      in_sec && /^gameid=/ { hasg = 1 }
      in_sec && /^path=/ { hasp = 1 }
      { print }
      END { if (in_sec) close_sec(); for (; blank > 0; blank--) print ""
            if (!found) { print ""; print sec; hasg = hasp = 0; close_sec() } }' "$INI" > "$INI.tmp" && mv "$INI.tmp" "$INI"
  esac
  # Monkey Island 2: ScummVM accepts any answer to the code wheel; type one automatically
  # (only on a fresh start: loading the resume slot skips the protection)
  [ "$ID" = monkey2 ] && [ -z "$SLOT" ] && /usr/local/sbin/mi2-copyprotect >/dev/null 2>&1 &
}
[ "$SYS" = scummvm ] && scumm_prep

if grep -q "^\[$SYS\]" /storage/.config/rgarch/systems.cfg 2>/dev/null; then
  echo "$(date +%T) rgarch $SYS rom=$ROM resume=${RESUME:-0} fresh=${FRESH:-0}"
  export RGARCH_TITLE="$TITLE"
  if [ "$SYS" = scummvm ]; then
    # not exec: Neustart (exit code 3) starts the game again without the resume save
    A=; [ "$RESUME" = 1 ] && A=--resume; [ "$FRESH" = 1 ] && A=--fresh
    while /usr/local/bin/rgarch $A "$SYS" "$ROM"; [ $? = 3 ]; do
      A=; echo "$(date +%T) rgarch $SYS restart"; scumm_prep
    done
    exit 0
  fi
  if [ "$RESUME" = 1 ]; then exec /usr/local/bin/rgarch --resume "$SYS" "$ROM"; fi
  if [ "$FRESH" = 1 ]; then exec /usr/local/bin/rgarch --fresh "$SYS" "$ROM"; fi
  exec /usr/local/bin/rgarch "$SYS" "$ROM"
fi

# --- RetroArch until rgarch phase 2 ---
case "$SYS" in
  n64)       CORE=mupen64plus_next ;;
  nds)       CORE=melondsds ;;
  psp)       CORE=ppsspp ;;
  dreamcast) CORE=flycast ;;
  *) echo "no emulator for $SYS"; exit 1 ;;
esac
echo "$(date +%T) retroarch $SYS core=$CORE rom=$ROM resume=${RESUME:-0}"
EXTRA=; [ "$RESUME" = 1 ] && EXTRA="--appendconfig /storage/.config/retroarch/resume.cfg"
exec retroarch --verbose --config /storage/.config/retroarch/retroarch.cfg $EXTRA \
  -L "/storage/.config/retroarch/cores/${CORE}_libretro.so" "$ROM"
