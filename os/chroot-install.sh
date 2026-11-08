#!/system/bin/sh
# Runs on Android as root (pushed by os/stage step): chroot into the Debian tree at /data/local/tmp/deb and install the package set.
R=/data/local/tmp/deb
for m in proc sys dev dev/pts; do mkdir -p $R/$m; done
mountpoint -q $R/proc || mount -t proc proc $R/proc
mountpoint -q $R/sys || mount -t sysfs sys $R/sys
mountpoint -q $R/dev || mount -o bind /dev $R/dev
mountpoint -q $R/dev/pts || mount -t devpts devpts $R/dev/pts
echo "nameserver 192.168.178.1" > $R/etc/resolv.conf
cat > $R/etc/apt/sources.list.d/debian.sources <<'X'
Types: deb
URIs: http://deb.debian.org/debian
Suites: trixie trixie-updates
Components: main contrib non-free non-free-firmware
Signed-By: /usr/share/keyrings/debian-archive-keyring.gpg

Types: deb
URIs: http://deb.debian.org/debian-security
Suites: trixie-security
Components: main contrib non-free non-free-firmware
Signed-By: /usr/share/keyrings/debian-archive-keyring.gpg
X
rm -f $R/etc/apt/apt.conf.d/docker-* 
env -i PATH=/usr/sbin:/usr/bin:/sbin:/bin HOME=/root TERM=dumb TMPDIR=/tmp DEBIAN_FRONTEND=noninteractive chroot $R /bin/bash -c '
set -e
# Android: apt sandbox user cannot reach the network
echo "APT::Sandbox::User \"root\";" > /etc/apt/apt.conf.d/00rg353
dpkg --configure -a
apt-get update
apt-get install -y --no-install-recommends \
  systemd systemd-sysv systemd-resolved systemd-timesyncd udev dbus kmod initramfs-tools \
  iwd openssh-server firmware-realtek wireless-regdb ca-certificates iproute2 iputils-ping \
  less nano procps psmisc alsa-utils e2fsprogs dosfstools util-linux bash-completion \
  retroarch libretro-gambatte libretro-mgba libretro-nestopia libretro-snes9x libretro-genesisplusgx \
  scummvm libsdl2-2.0-0 libsdl2-ttf-2.0-0 libgl1-mesa-dri libegl1 libgles2 libgbm1 mesa-utils \
  fonts-dejavu-core python3-minimal python3-evdev libopengl0 kexec-tools libsdl2-image-2.0-0 unzip 7zip bluez bluez-alsa-utils libasound2-plugin-bluez
apt-get clean
echo INSTALL_OK
'
