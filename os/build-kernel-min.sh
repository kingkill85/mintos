#!/usr/bin/env bash
# Minimal RG353V kernel: localyesconfig from the device's modules (built in, no initramfs),
# all non-Rockchip platforms off, Wi-Fi/Bluetooth as modules. Version "-rg353v-min".
#   ./os/build-kernel-min.sh   -> ~/rg353-build/out-min/{Image,modules}
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
W=${RG353_BUILD:-$HOME/rg353-build}
SRC=$W/linux-6.12.116
VER=${MINVER:--rg353v-min}
OUT=$W/out${VER#-rg353v}
cp "$W/full.config" "$SRC/.config"
# st7703 RG353V-V2 init: VCOM/POWER_EXT from the BSP (upstream patch by Ao Zhong, dri-devel 2024-02,
# never merged); with the old values the panel sometimes stays black after power-on
grep -q "SETVCOM, 0x7f, 0x7f" "$SRC/drivers/gpu/drm/panel/panel-sitronix-st7703.c" || patch -d "$SRC" -p1 < "$HERE/patches/st7703-rg353v2-init.patch"
nix-shell -p pkgsCross.aarch64-multiplatform.stdenv.cc bc flex bison openssl elfutils perl python3 kmod dtc --run "
set -e
cd $SRC
export ARCH=arm64 CROSS_COMPILE=aarch64-unknown-linux-gnu-
(yes '' || true) | make LSMOD=$W/lsmod-builtin.txt localyesconfig >/dev/null
scripts/kconfig/merge_config.sh -m .config $HERE/kernel-min.fragment >/dev/null
scripts/config --set-str LOCALVERSION $VER
make olddefconfig >/dev/null
for k in IIO_MUX MUX_GPIO TOUCHSCREEN_HYNITRON_CSTXXX DRM_PANEL_SITRONIX_ST7703 DRM_PANEL_NEWVISION_NV3051D JOYSTICK_ADC INPUT_PWM_VIBRA DRM_PANFROST SND_SOC_RK817 MMC_DW_ROCKCHIP EXT4_FS REGULATOR_RK808 I2C_RK3X GPIO_ROCKCHIP ROCKCHIP_IODOMAIN USB_F_NCM; do grep -qE \"^CONFIG_\$k=y\" .config || { echo \"not built in: \$k\"; exit 1; }; done
grep -qE '^CONFIG_RTW88_8821CS=m' .config || { echo 'rtw88 not a module'; exit 1; }
echo modules: \$(grep -c '=m\$' .config) builtin: \$(grep -c '=y\$' .config)
make -j\$(nproc) Image modules
rm -rf $OUT && mkdir -p $OUT
cp arch/arm64/boot/Image $OUT/
make INSTALL_MOD_PATH=$OUT/modules INSTALL_MOD_STRIP=1 modules_install >/dev/null
"
rm -f "$OUT"/modules/lib/modules/*/build "$OUT"/modules/lib/modules/*/source
echo "minimal kernel built: $OUT"
