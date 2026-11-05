/** @file
  Galaxy J3 2017 (SM-J330F, Exynos 7570) eMMC platform glue for DwMmcHcDxe.

  DT (j3y17lte rev04): dwmmc0@13540000, "samsung,exynos-dw-mshc", 8-bit,
  non-removable, broken-card-detect, clock-frequency 700 MHz, ciu-div 3
  -> bus_hz = 700 / (3 + 1) = 175 MHz.

  Bring-up policy: report a slightly HIGHER base clock (200 MHz) than the
  expected 175 MHz so every computed card clock is <= the requested one,
  and only advertise default speed (no HS/HS200/DDR) so the card stays in
  legacy timing that the bootloader's CLKSEL setting can sample safely.
**/

#include <Uefi.h>
#include <IndustryStandard/Acpi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/DevicePath.h>
#include <Protocol/NonDiscoverableDevice.h>
#include <Protocol/PlatformDwMmc.h>

#define J3_EMMC_BASE        0x13540000
#define J3_EMMC_SIZE        0x2000
#define J3_EMMC_BASE_CLK    200000      // kHz (DwMmcHcDxe uses kHz), see note above

#pragma pack (1)
typedef struct {
  VENDOR_DEVICE_PATH          Vendor;
  UINT64                      BaseAddress;
  EFI_DEVICE_PATH_PROTOCOL    End;
} J3_EMMC_DEVICE_PATH;

typedef struct {
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR  Mmio;
  EFI_ACPI_END_TAG_DESCRIPTOR        End;
} J3_EMMC_RESOURCES;
#pragma pack ()

STATIC J3_EMMC_DEVICE_PATH mJ3EmmcDevicePath = {
  {
    { HARDWARE_DEVICE_PATH, HW_VENDOR_DP,
      { (UINT8)(sizeof (VENDOR_DEVICE_PATH) + sizeof (UINT64)),
        (UINT8)((sizeof (VENDOR_DEVICE_PATH) + sizeof (UINT64)) >> 8) } },
    // reuse the DwMmc device-type GUID as the vendor GUID
    { 0x971ab768, 0xd733, 0x41be, { 0xac, 0x9e, 0x82, 0x36, 0x10, 0x94, 0xc9, 0x3c } }
  },
  J3_EMMC_BASE,
  { END_DEVICE_PATH_TYPE, END_ENTIRE_DEVICE_PATH_SUBTYPE,
    { sizeof (EFI_DEVICE_PATH_PROTOCOL), 0 } }
};

STATIC J3_EMMC_RESOURCES mJ3EmmcResources = {
  {
    ACPI_ADDRESS_SPACE_DESCRIPTOR,                       // Desc
    (UINT16)(sizeof (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR) - 3), // Len
    ACPI_ADDRESS_SPACE_TYPE_MEM,                         // ResType
    0,                                                   // GenFlag
    0,                                                   // SpecificFlag
    32,                                                  // AddrSpaceGranularity
    J3_EMMC_BASE,                                        // AddrRangeMin
    J3_EMMC_BASE + J3_EMMC_SIZE - 1,                     // AddrRangeMax
    0,                                                   // AddrTranslationOffset
    J3_EMMC_SIZE                                         // AddrLen
  },
  { ACPI_END_TAG_DESCRIPTOR, 0 }
};

STATIC NON_DISCOVERABLE_DEVICE mJ3EmmcDevice = {
  &gDwMmcHcNonDiscoverableDeviceGuid,
  NonDiscoverableDeviceDmaTypeNonCoherent,
  NULL,
  (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *)&mJ3EmmcResources
};

STATIC
EFI_STATUS
EFIAPI
J3DwMmcGetCapability (
  IN     EFI_HANDLE          Controller,
  IN     UINT8               Slot,
     OUT DW_MMC_HC_SLOT_CAP  *Capability
  )
{
  if ((Capability == NULL) || (Slot != 0)) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (Capability, sizeof (*Capability));
  Capability->DefaultSpeed = 1;
  Capability->HighSpeed    = 1;   // SDR HS up to 52 MHz, 8-bit (no DDR/HS200: needs tuning)
  Capability->Sdr104       = 0;
  Capability->Ddr50        = 0;
  Capability->SysBus64     = 0;   // all RAM is below 4 GiB
  Capability->BusWidth     = 8;
  Capability->SlotType     = EmbeddedSlot;
  Capability->CardType     = EmmcCardType;
  Capability->Voltage18    = 1;   // VCCQ fixed 1.8 V (mmc-*-1_8v, fixed_voltage)
  Capability->BaseClkFreq  = J3_EMMC_BASE_CLK;
  Capability->Controller   = Controller;

  DEBUG ((DEBUG_ERROR, "J3 eMMC: GetCapability base=%u kHz width=%u\n",
          Capability->BaseClkFreq, Capability->BusWidth));
  return EFI_SUCCESS;
}

STATIC
BOOLEAN
EFIAPI
J3DwMmcCardDetect (
  IN EFI_HANDLE  Controller,
  IN UINT8       Slot
  )
{
  // eMMC is soldered (non-removable); CDETECT is unreliable (broken-card-detect).
  return (Slot == 0);
}

STATIC PLATFORM_DW_MMC_PROTOCOL mJ3PlatformDwMmc = {
  J3DwMmcGetCapability,
  J3DwMmcCardDetect
};

EFI_STATUS
EFIAPI
J3DwMmcPlatformDxeInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  DevHandle;

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &ImageHandle,
                  &gPlatformDwMmcProtocolGuid, &mJ3PlatformDwMmc,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "J3 eMMC: install PlatformDwMmc failed: %r\n", Status));
    return Status;
  }

  DevHandle = NULL;
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &DevHandle,
                  &gEdkiiNonDiscoverableDeviceProtocolGuid, &mJ3EmmcDevice,
                  &gEfiDevicePathProtocolGuid,              &mJ3EmmcDevicePath,
                  NULL
                  );
  DEBUG ((DEBUG_ERROR, "J3 eMMC: registered DW-MSHC @0x%x: %r\n", J3_EMMC_BASE, Status));
  return Status;
}
