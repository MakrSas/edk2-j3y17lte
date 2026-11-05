# edk2-j3y17lte — UEFI for Samsung Galaxy J3 2017 (SM-J330F)

AArch64 UEFI firmware (edk2) for the **Samsung Galaxy J3 2017, SM-J330F**
(`j3y17lte`, Exynos 7570, board rev 04), booted by the stock Samsung bootloader
in place of the Linux kernel. The long-term goal is booting Windows on ARM.

Based on [vizPilot/edk2-exynos7570](https://github.com/vizPilot/edk2-exynos7570)
(itself derived from edk2-exynos7885; the package is still called `EXYNOS7885Pkg`).

## Status

| Feature | Status |
|---|---|
| Boot from stock S-Boot (repacked `boot.img`) | ✅ |
| Framebuffer / GOP, 720×1280, full-screen text console | ✅ |
| UEFI Shell | ✅ |
| Architected timer (26 MHz, CNTFRQ not set by S-Boot) | ✅ |
| eMMC (DW-MSHC 2.70a, 64-bit IDMAC), 8-bit HS, GPT with all 25 partitions | ✅ |
| Boot-stage colour markers (blue → yellow → green → red) | ✅ |
| Hardware buttons as input | ❌ |
| Second RAM bank (0x80000000–0xBFFFFFFF) | ❌ not mapped yet |
| ACPI tables for Exynos 7570 (MADT 4 cores, GTDT, DSDT) | ❌ still 7885 tables |
| SMP / PSCI (SMC, 4× Cortex-A53) | ❌ not wired up |
| USB, touch, battery, everything else | ❌ |

## Hardware notes

| Item | Value |
|---|---|
| SoC / CPU | Exynos 7570, 4× Cortex-A53 r0p4, MPIDR 0–3, PSCI via `smc` |
| Entry EL | **EL1** (AArch64); no EL2 is available |
| RAM | bank0 `0x40000000–0x7E3FFFFF`, bank1 `0x80000000–0xBFFFFFFF` (2 GB total) |
| Framebuffer | `0x67000000`, 720×1280 visible (virtual 720×2560), DECON at `0x14830000` |
| GIC-400 | GICD `0x104E1000`, GICC `0x104E2000` |
| eMMC | DW-MSHC at `0x13540000`, HCON[27]=1 → 64-bit IDMAC, CLKSEL64 at `0xA8`, sdr-timing `<3 0 2 0>`, 700 MHz source, ciu-div 3 |
| eMMC chip | Samsung QE63MB, 16 GB (30777344 sectors) |
| Stock board name | `SRPQC17A001KU` |

## Changes compared to upstream

- `EXYNOS7885Pkg/Devices/j3y17lte.dsc`: device config (RAM, framebuffer, 26 MHz timer PCD).
- `Include/Configuration/DeviceMemoryMap.h`: contiguous, non-overlapping bank0 map.
  The old map reached the secure region at `0x7E800000` and caused data aborts.
- `Drivers/J3DwMmcPlatformDxe` (new): registers the DW-MSHC controller as a
  non-discoverable device and provides `PLATFORM_DW_MMC_PROTOCOL` (8-bit, HS, 1.8 V, 200 MHz base clock).
- `Drivers/DwMmcHcDxe`: 64-bit IDMAC descriptors/register layout (`DBADDRL/U`, `IDSTS64`, `IDINTEN64`),
  bounded waits instead of endless loops, CLKSEL64 timing setup for Exynos 7.
  Optional read benchmark: build with `-D J3_EMMC_BENCH`.
- `Drivers/GraphicsConsoleDxe`: mode 0 uses the full panel size (the old 120×50 mode did not fit 1280 px).
- `Library/PlatformBootManagerLib`: serial console removed from ConOut/ErrOut (ConSplitter had no
  common mode and the Shell asserted), plus full-screen colour stage markers for debugging without a UART.
- `EXYNOS7885Pkg.fdf`: DwcSdhciDxe dropped (the 7570 eMMC is DW-MSHC, not SDHCI).

## Building

Tested on Ubuntu 24.04 (x86_64).

```bash
sudo apt install build-essential uuid-dev iasl git nasm python3 gcc-aarch64-linux-gnu
cd ..
git clone https://github.com/tianocore/edk2 --recursive -b edk2-stable202302
git clone https://github.com/tianocore/edk2-platforms.git
cd edk2-j3y17lte
./firstrun.sh
./build-j3y17lte.sh path/to/stock_boot.img
```

`edk2`, `edk2-platforms` and this repository must sit side by side.

The result is `workspace/boot_uefi.img`. It is the stock `boot.img` with only the kernel
replaced by the UEFI (`tools/repack_boot.py`). Take the stock `boot.img` from your own
firmware or dump it from the device (BOOT is `mmcblk0p9` on SM-J330F): `dd if=/dev/block/mmcblk0p9 of=/sdcard/boot.img`.

## Flashing

> **Back up EFS first.** Without it you lose IMEI/baseband. From TWRP:
> `dd if=$(readlink -f /dev/block/platform/*/by-name/EFS) of=/sdcard/efs.img`, then copy it off the device.

1. Unlock the bootloader (OEM unlock) and install TWRP to RECOVERY.
2. Boot TWRP: **Vol Up + Home + Power**.
3. Flash the image with `dd` and **reboot from TWRP**:
   ```bash
   adb push workspace/boot_uefi.img /tmp/b.img
   adb shell 'dd if=/tmp/b.img of=/dev/block/mmcblk0p9 bs=1M && sync'
   adb reboot
   ```

Rebooting straight from Download Mode (Odin/Heimdall) leaves the display uninitialised,
so you only get a black screen. To go back to Android, flash the stock `boot.img` the same way.

On macOS, `adb` may need `ADB_LIBUSB=1`.

## Boot stage colours

There is no UART, so progress is shown with full-screen colours:

| Colour | Stage |
|---|---|
| Blue | BDS started |
| Yellow | Console ready, connecting all controllers |
| Green | ConnectAll done |
| Red | Boot manager done, the Shell follows |

## AArch32 research (Windows 10 Mobile)

Windows 10 Mobile needs AArch32 firmware and kernel. This was tested on the SM-J330F and is
**not possible** without control of EL3:

- The kernel/UEFI is entered at EL1 AArch64, and there is no EL2 to drop from.
- PSCI `CPU_ON` via SMC32 (`0x84000003`) still starts the secondary core in AArch64.
- S-Boot starts a payload with a zImage header (no `ARMd` magic) in AArch64 as well.
  The probe is `tools/mk_a32probe.py`: a polyglot that turns the screen green in A32 and red in A64.

## Credits

- [TianoCore edk2](https://github.com/tianocore/edk2)
- [vizPilot/edk2-exynos7570](https://github.com/vizPilot/edk2-exynos7570) and the edk2-exynos7885 project it is based on
- DwMmcHcDxe is derived from the edk2-platforms DesignWare MMC driver
