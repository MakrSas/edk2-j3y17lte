#!/bin/bash
# Build UEFI for Samsung Galaxy J3 2017 (SM-J330F, j3y17lte, Exynos 7570).
#
#   ./build-j3y17lte.sh                       -> workspace/UEFI (BootShim + FD)
#   ./build-j3y17lte.sh path/to/stock_boot.img -> also workspace/boot_uefi.img
#
# The stock boot.img is used as a container: only the kernel is replaced,
# ramdisk/DTB/board name and the SEANDROIDENFORCE trailer are kept, which is
# what the Samsung bootloader expects.
set -e
. build_common.sh
./build_bootshim.sh
GCC5_AARCH64_PREFIX=aarch64-linux-gnu- build -j$(nproc) -s -n 0 -a AARCH64 -t GCC5 -p EXYNOS7885Pkg/Devices/j3y17lte.dsc
cat BootShim/BootShim.bin workspace/Build/EXYNOS7885Pkg/DEBUG_GCC5/FV/EXYNOS7885PKG_UEFI.fd > workspace/UEFI
echo "UEFI image: workspace/UEFI"
if [ -n "$1" ]; then
  python3 tools/repack_boot.py "$1" workspace/UEFI workspace/boot_uefi.img
  echo "boot image: workspace/boot_uefi.img"
fi
