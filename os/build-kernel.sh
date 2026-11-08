#!/usr/bin/env bash
# Cross-build the RG353V kernel: Debian trixie arm64 config + os/kernel.fragment.
#   ./os/build-kernel.sh   -> ~/rg353-build/out/{Image,dtb,modules}
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
W=${RG353_BUILD:-$HOME/rg353-build}
SRC=$W/linux-6.12.116
OUT=$W/out
nix-shell -p pkgsCross.aarch64-multiplatform.stdenv.cc bc flex bison openssl elfutils perl python3 kmod dtc --run "
set -e
cd $SRC
export ARCH=arm64 CROSS_COMPILE=aarch64-unknown-linux-gnu-
cp $W/debian.config .config
scripts/kconfig/merge_config.sh -m .config $HERE/kernel.fragment >/dev/null
make olddefconfig >/dev/null
for k in DRM_PANEL_NEWVISION_NV3051D JOYSTICK_ADC INPUT_PWM_VIBRA DRM_PANFROST RTW88_8821CS; do grep -E \"^CONFIG_\$k=\" .config || { echo missing \$k; exit 1; }; done
make -j\$(nproc) Image modules dtbs
rm -rf $OUT && mkdir -p $OUT/dtb
cp arch/arm64/boot/Image $OUT/
cp arch/arm64/boot/dts/rockchip/rk3566-anbernic-rg353v.dtb $OUT/dtb/
# Wi-Fi SDIO (mmc@fe000000): SDR50 gives CRC errors (-84) with rtw88, run it at 25 MHz high-speed
fdtput -d $OUT/dtb/rk3566-anbernic-rg353v.dtb /mmc@fe000000 sd-uhs-sdr50
fdtput -t u $OUT/dtb/rk3566-anbernic-rg353v.dtb /mmc@fe000000 max-frequency 25000000
make INSTALL_MOD_PATH=$OUT/modules INSTALL_MOD_STRIP=1 modules_install >/dev/null
"
echo "kernel built: $OUT"
