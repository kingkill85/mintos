#!/bin/bash
# Runs on the RG353V (deploy.sh copies and starts it). Makes ROCKNIX quiet:
#  - RetroArch: no on-screen messages, menu only via F+X, quit via F+Start
#  - RetroArch: fixed player-1 bindings as core overrides (autoconfig does not
#    apply to the InputPlumber virtual DualSense, so A/B would be dead)
#  - sway: invisible cursor theme (start.sh activates it on every start)
# Originals are kept once as *.rglauncher-backup.
set -e
HERE=/storage/.config/rglauncher

RA=/storage/.config/retroarch/retroarch.cfg
cp -n "$RA" "$RA.rglauncher-backup"
set_cfg() { if grep -q "^$1 = " "$RA"; then sed -i "s|^$1 = .*|$1 = \"$2\"|" "$RA"; else echo "$1 = \"$2\"" >> "$RA"; fi; }
set_cfg input_menu_toggle_gamepad_combo 0
set_cfg input_enable_hotkey_btn 10
set_cfg input_menu_toggle_btn 2
set_cfg input_exit_emulator_btn 9
set_cfg video_font_enable false
set_cfg menu_enable_widgets false
for k in $(grep -oE "^notification_show_[a-z_]+" "$RA" | grep -v duration); do set_cfg "$k" false; done
set_cfg notification_show_screenshot_flash 0
set_cfg log_verbosity false
set_cfg log_to_file false

PAD="/tmp/joypads/Sony Interactive Entertainment DualSense Wireless Controller.cfg"
BINDS=$HERE/p1-binds.cfg
grep -E "^input_(a|b|x|y|l|r|l2|r2|l3|r3|select|start|up|down|left|right)_(btn|axis) |^input_(l|r)_(x|y)_(plus|minus)_axis " "$PAD" \
  | sed "s/^input_/input_player1_/" > "$BINDS"
echo 'input_player1_joypad_index = "0"' >> "$BINDS"
n=0
for info in /tmp/cores/*.info; do
  name=$(sed -n 's/^corename = "\(.*\)"/\1/p' "$info" | head -1)
  [ -n "$name" ] || continue
  f="/storage/.config/retroarch/config/$name/$name.cfg"
  if [ -f "$f" ] && ! grep -q "^# rglauncher" "$f"; then echo "keep existing $f"; continue; fi
  mkdir -p "$(dirname "$f")"
  { echo "# rglauncher: player 1 bindings (RetroArch autoconfig does not apply the InputPlumber pad)"; cat "$BINDS"; } > "$f"
  n=$((n + 1))
done
echo "retroarch: $n core overrides"

python3 $HERE/make-invisible-cursor.py /storage/.icons/none
# ROCKNIX rewrites the sway config at boot, so start.sh applies the theme via swaymsg.
echo "sway: invisible cursor theme installed"
