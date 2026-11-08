#!/usr/bin/env bash
# Write the staged rg353v OS from /data/local/tmp/deb onto the SD card in TF1 (device runs Android, root).
# DESTRUCTIVE: formats the SD's second partition (old ROCKNIX STORAGE) as "rootfs".
# The first partition (vfat, with the mainline U-Boot in front of it) is kept; extlinux.conf is replaced.
set -euo pipefail
T=$(mktemp)
cat > "$T" <<'EOF'
#!/system/bin/sh
set -e
R=/data/local/tmp/deb
for m in dev/pts dev sys proc; do umount $R/$m 2>/dev/null || true; done
for p in /data/local/tmp/rk /data/local/tmp/sys /data/local/tmp/sd /data/local/tmp/boot; do umount $p 2>/dev/null || true; done
mkfs.ext4 -F -q -L rootfs /dev/block/mmcblk1p2
mkdir -p /data/local/tmp/sd /data/local/tmp/boot
mount -t ext4 /dev/block/mmcblk1p2 /data/local/tmp/sd
(cd $R && tar -cf - .) | (cd /data/local/tmp/sd && tar -xpf -)
mount -t vfat /dev/block/mmcblk1p1 /data/local/tmp/boot
B=/data/local/tmp/boot
[ -f $B/extlinux/extlinux.conf.rocknix ] || mv $B/extlinux/extlinux.conf $B/extlinux/extlinux.conf.rocknix
cp $R/boot/Image $R/boot/initrd.img $R/boot/rk3566-anbernic-rg353v.dtb $B/
cp $R/boot/extlinux/extlinux.conf $B/extlinux/extlinux.conf
sync
ls -la $B $B/extlinux; df -h /data/local/tmp/sd
umount /data/local/tmp/boot; umount /data/local/tmp/sd
echo FLASH_OK
EOF
nix-shell -p android-tools --run "adb push $T /data/local/tmp/flash.sh >/dev/null && adb shell \"su -c 'sh /data/local/tmp/flash.sh'\""
