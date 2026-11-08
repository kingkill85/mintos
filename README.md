# MintOS (RG353V)

**MintOS**, an own minimal OS for the Anbernic RG353V: Debian 13 (trixie, arm64), a self-built
kernel, RetroArch/ScummVM and a plain game list (`rglauncher`). No desktop, no
compositor, no EmulationStation. It replaced ROCKNIX and the GammaOS Android.

## Layout

- `launcher/rglauncher.c`: the game list (C, SDL2 KMSDRM, SDL2_ttf, 640×480). It lists
  `/storage/roms/<system>`; D-pad moves, L/R or left/right page, **A** starts, **B** goes back.
  The top bar shows where you are ("MintOS", the system, "Einstellungen › WLAN") and the position.
  Starting a game replaces the process with `launch.sh`; systemd restarts the list afterwards.
- **Einstellungen** (last root entry, `launcher/settings.c`): brightness, volume, WLAN (networks,
  password via on-screen keyboard `launcher/keyboard.c`), Bluetooth (pair headphones/controllers),
  Spiel-Optionen (per system Bild/Filter/Rahmen in `rgarch/user.cfg` over `systems.cfg`), screen-off
  timer, info, reboot/power-off. Slow commands (`mintos-wifi`, `mintos-bt`) run as background jobs
  (`launcher/job.c`). Off switches stop the services and unload the drivers. State: `/storage/.config/mintos/settings.cfg`.
- Kernels (Linux 6.12.116, cross-built with `os/build-kernel*.sh`):
  - `Image-min4` (default, 20 MB): `MINVER=-rg353v-min4 os/build-kernel-min.sh` — localyesconfig
    from the device's modules, only Rockchip, no initramfs, no tracing/BPF/IPv6/netfilter/video
    decoders, plus the CST340 touchscreen and the stick ADC multiplexer; 22 modules (Wi-Fi/Bluetooth, whose firmware is on
    the rootfs; plus BT_HIDP/UHID for Bluetooth controllers). System ready about 9 s after the kernel
    starts. `Image-min3` is the previous one. Modules go to `/usr/lib/modules` (never extract a `lib/`
    into `/`: it replaces the merged-usr symlink and nothing can start any more).
  - `Image` + `initrd.img` (fallback): `os/build-kernel.sh` — Debian's arm64 config plus the
    RG353V drivers it lacks (NV3051D panel, adc-joystick, pwm-vibra).
  - Both use the DTB with the Wi-Fi SDIO limited to 25 MHz high-speed (SDR50 gave CRC errors).
- `os/files/`: everything installed on top of Debian: systemd units, iwd, sshd, RetroArch
  config, `launch.sh` (system → core), and helpers:
  - `rg353-pad`: merges the buttons, the F key and the sticks (separate kernel devices) into
    one uinput gamepad "RG353V Gamepad".
  - `wifi-check`: gets the RTL8821CS up after boot (re-probes the SDIO controller if the card
    was not detected, reloads rtw88 after firmware upload errors) until iwd connects.
  - `rg353-power`: power key like Garlic OS. During a game it remembers the game, lets
    RetroArch write its auto save state and powers off; on the next boot rglauncher starts
    that game again and loads the state. Outside games it just powers off.
  - `no-fbcon`: detaches and blanks the text console, so no kernel text shows between list and game.
  - `rg353-audio`: speaker/headphone switching (mainline leaves it on headphones), the
    volume buttons, and Bluetooth headphones (bluealsa becomes ALSA's default while connected).
  - `mintos-wifi`, `mintos-bt`: Wi-Fi (iwd D-Bus) and Bluetooth (bluetoothctl) for the settings.
  - `usb-net`: USB NCM gadget, the device is `10.55.0.2` (PC side `10.55.0.1/24`), SSH works without Wi-Fi.
- `os/chroot-install.sh`, `os/stage.sh`, `os/flash-sd.sh`: build the root filesystem on the
  device while it runs Android (adb root, no card reader needed) and write it to the SD card.
- `os/restore-android.sh`: the way back in when the SD's system is unreachable (maskrom ->
  Android on the eMMC -> adb root repairs the SD; the comments list the steps back to SD boot).
- Host tests: `frontend/test/run.sh` (C) and `python3 -I os/test/test_*.py` (helpers).

## rgarch (games)

`frontend/` is **rgarch**, a small libretro frontend with a MinUI-style menu. `launch.sh`
starts every system listed in `/storage/.config/rgarch/systems.cfg` in rgarch (core, scaling,
filter, overlay per system); n64, nds, psp and dreamcast still use RetroArch (phase 2).

- **F** opens the menu: Weiter, Speichern, Laden (9 slots with a preview picture, left/right),
  Disc wechseln (multi-disc games), Beenden.
- ScummVM has no savestates: Speichern/Laden use its quicksave slots (Alt/Ctrl + 1..9, menu
  slots 1-9 = ScummVM slots 1-9; slot 0 stays ScummVM's autosave); "ScummVM-Menü" opens its
  own menu (Ctrl+F5).
- Zipped/7z ROMs are extracted to /tmp for cores that cannot read archives.
- RetroArch's compressed states (#RZIP) load; a resume point that cannot be loaded is kept
  as `<game>.auto.failed` instead of being overwritten.
- Saves: `/storage/saves/<system>/<game>.srm` (+ `.rtc`); states:
  `/storage/states/<system>/<game>.st<n>` + `.st<n>.png`; power-key state `<game>.auto`.
  `rg353-migrate-saves` moved the old RetroArch files there.
- Core options stay in RetroArch's `.opt` files; log in `/var/log/rgarch.log`.
- Tests: `frontend/test/run.sh` (host, SDL dummy drivers, a test core);
  `frontend/test/device-smoke.sh` (on the device: start, menu, save/load slot 1, quit).

Video follows the Retro Game Corps guides (640×480): GB/GBC/Game Gear with frame overlays,
TV consoles full screen with a sharp filter, arcade/ScummVM in their own aspect, PS1 bilinear.

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

U-Boot's `bootdelay` is patched from 2 to 0 (`os/uboot-bootdelay0.py`, which also updates the
SHA-256 the SPL checks); the original area is in `/storage/.backup/uboot-itb-area.orig`.
The boot logo (`os/splash/`) is in the console framebuffer about 3 s after power-on.

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
`Image-min4` with its modules (`MINVER=-rg353v-min4 os/build-kernel-min.sh`). `device/` holds
the earlier ROCKNIX launcher setup.
