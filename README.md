# rg353v

An own minimal OS for the Anbernic RG353V: Debian 13 (trixie, arm64), a self-built
kernel, RetroArch/ScummVM and a plain game list (`rglauncher`). No desktop, no
compositor, no EmulationStation. It replaced ROCKNIX and the GammaOS Android.

## Layout

- `launcher/rglauncher.c`: the game list (C, SDL2 KMSDRM, SDL2_ttf, 640×480). It lists
  `/storage/roms/<system>`; D-pad moves, L/R or left/right page, **A** starts, **B** goes back.
  Starting a game replaces the process with `launch.sh`; systemd restarts the list afterwards.
- Kernels (Linux 6.12.116, cross-built with `os/build-kernel*.sh`):
  - `Image-min2` (default, 20 MB): `MINVER=-rg353v-min2 os/build-kernel-min.sh` — localyesconfig
    from the device's modules, only Rockchip, no initramfs, no tracing/BPF/IPv6/netfilter/video
    decoders, plus the CST340 touchscreen; 22 modules (Wi-Fi/Bluetooth, whose firmware is on
    the rootfs). System ready about 9 s after the kernel starts. `Image-min` is the previous one.
  - `Image` + `initrd.img` (fallback): `os/build-kernel.sh` — Debian's arm64 config plus the
    RG353V drivers it lacks (NV3051D panel, adc-joystick, pwm-vibra).
  - Both use the DTB with the Wi-Fi SDIO limited to 25 MHz high-speed (SDR50 gave CRC errors).
- `os/files/`: everything installed on top of Debian: systemd units, iwd, sshd, RetroArch
  config, `launch.sh` (system → core), and helpers:
  - `rg353-pad`: merges the buttons and the F key (separate kernel devices) into one
    uinput gamepad "RG353V Gamepad", so F can be RetroArch's hotkey.
  - `wifi-check`: gets the RTL8821CS up after boot (re-probes the SDIO controller if the card
    was not detected, reloads rtw88 after firmware upload errors) until iwd connects.
  - `rg353-audio`: speaker/headphone switching (mainline leaves it on headphones) and the
    volume buttons.
  - `usb-net`: USB NCM gadget, the device is `10.55.0.2` (PC side `10.55.0.1/24`), SSH works without Wi-Fi.
- `os/chroot-install.sh`, `os/stage.sh`, `os/flash-sd.sh`: build the root filesystem on the
  device while it runs Android (adb root, no card reader needed) and write it to the SD card.

## Controls in games (Retro Game Corps layout, F = hotkey)

| F + Start | quit to list | F + X | RetroArch menu |
|---|---|---|---|
| F + R1 / L1 | save / load state | F + R2 | fast forward |
| F + L2 | rewind | F + Y | FPS |
| F + B | reset | | |

Video follows the Retro Game Corps RG351V guide (same 640×480 screen): integer scaling
(not GBA), core aspect (SNES 4:3), `sharp-bilinear-2x-prescale`, J.dewitz 351V overlays
for GB/GBC/Game Gear. Saves (`.srm`) and states sit next to the ROMs.

## Everyday use

```sh
./build.sh    # cross-compile the launcher with zig (via nix-shell)
./deploy.sh   # copy launcher + os/files to the device, restart the list
```

SSH: `root@192.168.178.37` (key only). Set `RG353V=root@<ip>` for another address.

## Boot and recovery

The SD card in TF1 holds the mainline U-Boot (from ROCKNIX), a vfat partition (`/boot`)
and the ext4 `rootfs`. U-Boot runs `/boot/boot.scr` (source `os/files/boot/boot.cmd`):

- `rg353-try-kernel <Image> [<initrd>|none] [args]` boots another kernel **once**. The script
  clears the flag before booting it, so if it hangs: hold power 10 s, switch on, and the
  default kernel boots. A kernel that fails to load falls back on its own.
- If `boot.scr` itself fails, U-Boot finds `/boot/extlinux/extlinux.conf` on the rootfs
  partition and boots the full kernel from there (`rgboot=fallback` on the command line).

The eMMC boot area is zeroed so the SoC boots from SD. Back to Android: power off,
remove the SD card, connect USB, power on (maskrom) and write
`~/rg353-backup/emmc-first-8MB.img` at sector 0 and `emmc-uboot-mmcblk0p2.img` at
sector 16384 with `rkdeveloptool` (loader `MiniLoaderAll.bin`), as root.

Debugging: `fbgrab [out.png]` saves what the panel shows; `rg353-press BTN_MODE BTN_START`
injects buttons; the journal is persistent (`journalctl -b -1`).

## Known gaps

`os/stage.sh` + `os/flash-sd.sh` describe the first install (full kernel, extlinux on the
boot partition). For a fresh card, also: copy `os/files/boot/boot.scr` to the boot
partition, rename its `extlinux/extlinux.conf` away (it would win over `boot.scr`), put the
fallback `extlinux.conf` into the rootfs's `/boot/extlinux/`, set the card's PARTUUID in
`boot.cmd` (`mkimage -A arm64 -T script -C none -d boot.cmd boot.scr`), and install
`Image-min2` with its modules (`MINVER=-rg353v-min2 os/build-kernel-min.sh`). `device/` holds
the earlier ROCKNIX launcher setup.
