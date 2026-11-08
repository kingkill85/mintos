#!/bin/bash
# Replaces start_es.sh: same environment setup, then the minimal launcher instead of EmulationStation.
. /usr/bin/es_settings
HERE=/storage/.config/rglauncher

# ROCKNIX rewrites /storage/.config/sway/config on every boot (autostart 111-sway-init),
# so the pointer settings have to be applied to the running sway each time.
swaymsg 'seat * hide_cursor 1' >/dev/null 2>&1
swaymsg 'seat * xcursor_theme none 1' >/dev/null 2>&1

# ROCKNIX only unblocks the radio at boot; EmulationStation normally brings up Wi-Fi.
# Without it, make sure the saved network is connected, and keep a log on storage.
(
  for i in $(seq 1 30); do
    nmcli -t -f DEVICE,STATE dev 2>/dev/null | grep -q '^wlan0:connected$' && break
    [ "$i" = 5 ] || [ "$i" = 15 ] && wifictl connect
    sleep 2
  done
  {
    echo "=== $(date) boot wifi try $i"
    nmcli -t -f DEVICE,STATE,CONNECTION dev
    ip -br addr show wlan0
    systemctl is-active NetworkManager iwd sshd
    journalctl -b --no-pager -u NetworkManager -u iwd | tail -40
  } > "$HERE/network.log" 2>&1
) &

set_kill set "rglauncher"
scanout_reset_stale 2>/dev/null
exec "$HERE/rglauncher"
