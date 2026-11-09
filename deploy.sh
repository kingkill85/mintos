#!/usr/bin/env bash
# Update the launcher and its configs on the RG353V (own Debian OS, see os/) over SSH.
#   ./deploy.sh   copies build/rglauncher + os/files/{storage,etc,usr} and restarts the launcher
set -euo pipefail
cd "$(dirname "$0")"
HOST=${RG353V:-root@192.168.178.37}
scp -q build/rglauncher "$HOST:/storage/.config/rglauncher/rglauncher.new"
scp -q build/rgarch "$HOST:/usr/local/bin/rgarch.new" && ssh "$HOST" 'mv /usr/local/bin/rgarch.new /usr/local/bin/rgarch'
# system tools (tools/, C): unpacked next to the running ones, then renamed over them
tar -C build/tools --owner=0 --group=0 --exclude=gen -cf - . | ssh "$HOST" 'set -e; mkdir -p /tmp/mintos-tools; tar -C /tmp/mintos-tools -xf -
  for f in /tmp/mintos-tools/*; do cp -P "$f" "/usr/local/sbin/.$(basename "$f")" && mv -T "/usr/local/sbin/.$(basename "$f")" "/usr/local/sbin/$(basename "$f")"; done
  rm -rf /tmp/mintos-tools'
tar -C os/files --owner=0 --group=0 -cf - storage etc usr | ssh "$HOST" 'tar -C / -xf - && systemctl daemon-reload
  # mintosd replaced the Python daemons rg353-pad/-audio/-power
  if [ -e /etc/systemd/system/rg353-pad.service ]; then
    systemctl disable -q --now rg353-pad rg353-audio rg353-power
    rm -f /usr/local/sbin/rg353-pad /usr/local/sbin/rg353-audio /usr/local/sbin/rg353-power /etc/systemd/system/rg353-pad.service /etc/systemd/system/rg353-audio.service /etc/systemd/system/rg353-power.service
    systemctl daemon-reload && systemctl enable -q mintosd
  fi
  systemctl restart mintosd'
ssh "$HOST" 'mv /storage/.config/rglauncher/rglauncher.new /storage/.config/rglauncher/rglauncher && sync
  if pgrep -x rgarch >/dev/null || pgrep -x retroarch >/dev/null; then echo "game running: launcher updates when it exits"
  else systemctl restart rglauncher && echo deployed; fi'
