#!/usr/bin/env bash
# Restore the RG353V eMMC boot area (Android) from ~/rg353-backup via maskrom: the way back in
# when the SD's Debian is unreachable. Then, in Android with the SD inserted:
#   adb root; adb shell; mount -t ext4 /dev/block/mmcblk1p2 /data/local/tmp/r  -> repair the SD
#   umount, then let the SoC boot from SD again (clears the eMMC boot area):
#   dd if=/dev/zero of=/dev/block/mmcblk0 bs=512 seek=64 count=8128
#   dd if=/dev/zero of=/dev/block/mmcblk0p2 bs=1M count=4; sync; adb reboot
set -euo pipefail
B=${RG353_BACKUP:-/home/michael/rg353-backup}   # absolute: runs under sudo
RK=$B/rkdeveloptool/bin/rkdeveloptool   # nix-build '<nixpkgs>' -A rkdeveloptool -o ~/rg353-backup/rkdeveloptool (GC root)
echo "Warte auf das Geraet im Flashmodus (SD raus, OTG am PC, einschalten)..."
until $RK ld 2>/dev/null | grep -q DevNo; do sleep 1; done
$RK ld
$RK db "$B/MiniLoaderAll.bin"
sleep 1
$RK wl 0 "$B/emmc-first-8MB.img"
$RK wl 16384 "$B/emmc-uboot-mmcblk0p2.img"
$RK rd
echo "FERTIG - Android sollte jetzt starten."
