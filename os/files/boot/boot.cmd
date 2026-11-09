# rg353v boot script for the SD card's U-Boot (2026.01, bootmeth "script").
# Boots the kernel named in "kernel"/"initrd". To try another kernel exactly once,
# Linux writes "1" to boot-test and the settings to boot-test.env (rg353-try-kernel).
# This script flips the flag to "0" before booting the test kernel, so if it hangs,
# a power cycle boots the default kernel again.
setenv tmp 0x00b00000
# default: minimal kernel, no initramfs (Image/initrd.img = full Debian-config kernel, fallback)
setenv kernel Image-min5
setenv initrd none
setenv dtb rk3566-anbernic-rg353v.dtb
setenv args "root=PARTUUID=0e11789e-45d0-4dd8-8a53-e9342b0f5faa rootwait rw console=ttyS2,1500000 console=tty3 quiet loglevel=3 vt.global_cursor_default=0"
setenv mode default
if fatload ${devtype} ${devnum}:${distro_bootpart} ${tmp} boot-test 1; then
  if itest.b *${tmp} == 0x31; then
    mw.b ${tmp} 0x30 1
    fatwrite ${devtype} ${devnum}:${distro_bootpart} ${tmp} boot-test 1
    if fatload ${devtype} ${devnum}:${distro_bootpart} ${tmp} boot-test.env; then
      env import -t ${tmp} ${filesize}
      setenv mode test
    fi
  fi
fi
setenv bootargs "${args} rgboot=${mode}"
fatload ${devtype} ${devnum}:${distro_bootpart} ${kernel_addr_r} ${kernel}
fatload ${devtype} ${devnum}:${distro_bootpart} ${fdt_addr_r} ${dtb}
if itest.s "${initrd}" == "none"; then
  booti ${kernel_addr_r} - ${fdt_addr_r}
else
  fatload ${devtype} ${devnum}:${distro_bootpart} ${ramdisk_addr_r} ${initrd}
  booti ${kernel_addr_r} ${ramdisk_addr_r}:${filesize} ${fdt_addr_r}
fi
