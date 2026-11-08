#!/usr/bin/env bash
# Stage the rg353v OS inside the Debian tree on the device while it runs Android (root, adb).
# Non-destructive: only touches /data/local/tmp/deb on the internal storage.
#   needs: ~/rg353-build/out (kernel), build/rglauncher, a ROCKNIX storage backup tar
set -euo pipefail
cd "$(dirname "$0")/.."
W=${RG353_BUILD:-$HOME/rg353-build}
BACKUP=$(ls -d ~/rg353-backup/rocknix-storage-* | tail -1)/storage.tar
ST=$W/stage
rm -rf "$ST" && mkdir -p "$ST/root"
cp -a os/files/. "$ST/root/"
mkdir -p "$ST/root/storage/.config/rglauncher/fonts" "$ST/root/root/.ssh" "$ST/root/var/lib/iwd" "$ST/root/usr/lib"
cp build/rglauncher "$ST/root/storage/.config/rglauncher/"
cp device/fonts/* "$ST/root/storage/.config/rglauncher/fonts/"
cp ~/.ssh/id_ed25519.pub "$ST/root/root/.ssh/authorized_keys"
cp -a "$W/out/modules/lib/modules" "$ST/root/usr/lib/"
rm -f "$ST"/root/usr/lib/modules/*/build "$ST"/root/usr/lib/modules/*/source
cp "$W/out/Image" "$W/out/dtb/rk3566-anbernic-rg353v.dtb" "$ST/root/boot/"
# Wi-Fi: reuse the network ROCKNIX had saved
psk=$(tar -xOf "$BACKUP" .config/NetworkManager/system-connections/Tardis.nmconnection | sed -n 's/^psk=//p')
printf '[Security]\nPassphrase=%s\n' "$psk" > "$ST/root/var/lib/iwd/Tardis.psk"
chmod 600 "$ST/root/var/lib/iwd/Tardis.psk"
# games, saves (.srm next to the ROMs) and launcher state
mkdir -p "$ST/root/storage"
tar -xf "$BACKUP" -C "$ST" games-internal/roms .config/rglauncher/recent .config/rglauncher/state 2>/dev/null || true
mv "$ST/games-internal/roms" "$ST/root/storage/roms"
cp "$ST/.config/rglauncher/recent" "$ST/.config/rglauncher/state" "$ST/root/storage/.config/rglauncher/" 2>/dev/null || true
# RetroArch: libretro GLSL shaders and J.dewitz's 351V overlays (Retro Game Corps RG351V guide)
RA=$ST/root/storage/.config/retroarch
mkdir -p "$RA/shaders" "$RA/overlays"
curl -sL https://github.com/libretro/glsl-shaders/archive/refs/heads/master.zip -o "$ST/glsl.zip"
unzip -q "$ST/glsl.zip" -d "$ST" && cp -a "$ST"/glsl-shaders-master/. "$RA/shaders/"
curl -sL https://retrogamecorps.com/wp-content/uploads/2021/05/351v-overlays.zip -o "$ST/ov.zip"
unzip -q "$ST/ov.zip" -d "$ST/ov" && cp -a "$ST/ov/GB-GBC" "$ST/ov/GG" "$ST/ov/NGP" "$RA/overlays/"
# current libretro cores + info files (Debian's Gambatte is from 2016, mGBA lacks colour options)
mkdir -p "$RA/cores" "$RA/info"
for c in gambatte mgba nestopia snes9x genesis_plus_gx picodrive fbneo mednafen_pce_fast mednafen_ngp mednafen_wswan handy stella2014 prosystem mednafen_vb pcsx_rearmed mupen64plus_next melondsds ppsspp flycast scummvm; do
  curl -sL "https://buildbot.libretro.com/nightly/linux/aarch64/latest/${c}_libretro.so.zip" -o "$ST/$c.zip"
  unzip -q -o "$ST/$c.zip" -d "$RA/cores"
done
# ScummVM core's engine data and themes -> system dir
curl -sL https://buildbot.libretro.com/assets/system/ScummVM.zip -o "$ST/scummvm-sys.zip"
mkdir -p "$ST/root/storage/roms/bios" && unzip -q -o "$ST/scummvm-sys.zip" -d "$ST/root/storage/roms/bios"
curl -sL https://buildbot.libretro.com/assets/frontend/info.zip -o "$ST/info.zip" && unzip -q -o "$ST/info.zip" -d "$RA/info"
# Retro Game Corps overlay pack (Jeltron/Perfect overlays, sharp-shimmerless), 2024 shader guide
curl -sL https://github.com/retrogamecorps/RGC-Overlay-Pack/archive/refs/tags/v1.0.zip -o "$ST/rgc.zip"
unzip -q "$ST/rgc.zip" -d "$ST"
mkdir -p "$RA/overlays/rgc" "$RA/shaders/rgc"
cp -a "$ST/RGC-Overlay-Pack-1.0/RGC overlays/." "$RA/overlays/rgc/"
cp -a "$ST/RGC-Overlay-Pack-1.0/RGC shaders/shaders_glsl/." "$RA/shaders/rgc/"
tar -C "$ST/root" --owner=0 --group=0 -cf "$ST/overlay.tar" .

cat > "$ST/configure.sh" <<'EOF'
#!/system/bin/sh
R=/data/local/tmp/deb
tar -xf /data/local/tmp/overlay.tar -C $R
env -i PATH=/usr/sbin:/usr/bin:/sbin:/bin HOME=/root TERM=dumb TMPDIR=/tmp chroot $R /bin/bash -c '
set -e
chmod 700 /root/.ssh /var/lib/iwd; chmod 600 /root/.ssh/authorized_keys
chmod 755 /storage/.config/rglauncher/launch.sh /storage/.config/rglauncher/rglauncher /usr/local/sbin/usb-net
ln -sf /run/systemd/resolve/stub-resolv.conf /etc/resolv.conf
KV=$(ls /lib/modules | head -1)
depmod -a "$KV"
echo "MODULES=most" > /etc/initramfs-tools/conf.d/rg353v
update-initramfs -c -k "$KV" >/dev/null
cp /boot/initrd.img-"$KV" /boot/initrd.img
systemctl enable iwd systemd-resolved systemd-timesyncd ssh rglauncher usb-net wifi-check rg353-pad rg353-audio rg353-power >/dev/null 2>&1
systemctl mask getty@tty1 >/dev/null 2>&1
passwd -l root >/dev/null
rm -f /etc/apt/apt.conf.d/00rg353
echo CONFIGURE_OK "$KV"
'
EOF
nix-shell -p android-tools --run "
adb push '$ST/overlay.tar' '$ST/configure.sh' /data/local/tmp/ >/dev/null
adb shell \"su -c 'sh /data/local/tmp/configure.sh'\""
