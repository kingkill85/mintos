#!/usr/bin/env bash
# Update the launcher and its configs on the RG353V (own Debian OS, see os/) over SSH.
#   ./deploy.sh   copies build/rglauncher + os/files/{storage,etc,usr} and restarts the launcher
set -euo pipefail
cd "$(dirname "$0")"
HOST=${RG353V:-root@192.168.178.37}
scp -q build/rglauncher "$HOST:/storage/.config/rglauncher/rglauncher.new"
scp -q build/rgarch "$HOST:/usr/local/bin/rgarch.new" && ssh "$HOST" 'mv /usr/local/bin/rgarch.new /usr/local/bin/rgarch'
tar -C os/files --owner=0 --group=0 -cf - storage etc usr | ssh "$HOST" 'tar -C / -xf - && systemctl daemon-reload'
ssh "$HOST" 'mv /storage/.config/rglauncher/rglauncher.new /storage/.config/rglauncher/rglauncher && sync
  if pgrep -x rgarch >/dev/null || pgrep -x retroarch >/dev/null; then echo "game running: launcher updates when it exits"
  else systemctl restart rglauncher && echo deployed; fi'
