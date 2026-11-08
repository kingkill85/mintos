#!/bin/sh
# On-device acceptance for rgarch (run on the RG353V): device-smoke.sh <system> <rom> <outdir>
# start via launch.sh, screenshot, F -> menu, save slot 1, load slot 1, Beenden; checks files and exit time.
SYS=$1; ROM=$2; OUT=${3:-/tmp/smoke}; mkdir -p "$OUT"
name=$(basename "$ROM"); name=${name%.*}
fail=0; ok() { echo "ok   $*"; }; bad() { echo "FAIL $*"; fail=1; }
systemctl stop rglauncher
rm -f "/storage/states/$SYS/$name.st1" "/storage/states/$SYS/$name.st1.png"
/storage/.config/rglauncher/launch.sh "$ROM" "$SYS" >"$OUT/$SYS.log" 2>&1 &
sleep 8
pgrep -x rgarch >/dev/null && ok "$SYS running" || bad "$SYS not running"
fbgrab "$OUT/$SYS-game.png" >/dev/null
rg353-press BTN_MODE; sleep 1; fbgrab "$OUT/$SYS-menu.png" >/dev/null
rg353-press BTN_DPAD_DOWN; sleep 0.3; rg353-press BTN_DPAD_RIGHT; sleep 0.5
fbgrab "$OUT/$SYS-save.png" >/dev/null
rg353-press BTN_EAST; sleep 1.5
[ -f "/storage/states/$SYS/$name.st1" ] && ok "state slot 1" || bad "no state slot 1"
[ -f "/storage/states/$SYS/$name.st1.png" ] && ok "preview slot 1" || bad "no preview slot 1"
rg353-press BTN_MODE; sleep 1; rg353-press BTN_DPAD_DOWN; sleep 0.3; rg353-press BTN_DPAD_DOWN; sleep 0.3
fbgrab "$OUT/$SYS-load.png" >/dev/null
rg353-press BTN_EAST; sleep 1
grep -q "load state 1: ok" /var/log/rgarch.log && ok "load slot 1" || bad "load slot 1"
rg353-press BTN_MODE; sleep 1; rg353-press BTN_DPAD_UP; sleep 0.3
t0=$(date +%s%N); rg353-press BTN_EAST
while pgrep -x rgarch >/dev/null; do sleep 0.05; done
ms=$(( ($(date +%s%N) - t0) / 1000000 ))
[ $ms -lt 2000 ] && ok "quit in ${ms} ms" || bad "quit took ${ms} ms"
systemctl start rglauncher
exit $fail
