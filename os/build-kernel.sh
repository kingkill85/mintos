#!/usr/bin/env bash
# Cross-build the RG353V kernel: Debian trixie arm64 config + os/kernel.fragment.
#   ./os/build-kernel.sh   -> ~/rg353-build/out/{Image,dtb,modules}
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
W=${RG353_BUILD:-$HOME/rg353-build}
SRC=$W/linux-6.12.116
OUT=$W/out
# st7703 RG353V-V2 init: VCOM/POWER_EXT from the BSP (upstream patch by Ao Zhong, dri-devel 2024-02,
# never merged); with the old values the panel sometimes stays black after power-on
grep -q "SETVCOM, 0x7f, 0x7f" "$SRC/drivers/gpu/drm/panel/panel-sitronix-st7703.c" || patch -d "$SRC" -p1 < "$HERE/patches/st7703-rg353v2-init.patch"
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
# U-Boot turns the panel into anbernic,rg353v-panel-v2 (st7703), which asks for vcc/iovcc,
# not the nv3051d vdd-supply: without them vcc3v3_lcd0_n counts as unused and is switched
# off after 30 s, the panel goes black
P=\$(fdtget -t u $OUT/dtb/rk3566-anbernic-rg353v.dtb /regulator-vcc3v3-lcd0 phandle)
fdtput -t u $OUT/dtb/rk3566-anbernic-rg353v.dtb /dsi@fe060000/panel@0 vcc-supply \$P
fdtput -t u $OUT/dtb/rk3566-anbernic-rg353v.dtb /dsi@fe060000/panel@0 iovcc-supply \$P
make INSTALL_MOD_PATH=$OUT/modules INSTALL_MOD_STRIP=1 modules_install >/dev/null
"
echo "kernel built: $OUT"
