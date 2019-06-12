/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvddk_fuse.h"
#include "nvassert.h"
#include "nvrm_drf.h"
#include "nvrm_hardware_access.h"
#include "t30/nvboot_bct.h"
#include "t30/arfuse.h"
#include "nvddk_fuse_common.h"
#include "nvddk_fuse_write_priv.h"
#include "nvboot_misc_t30.h"
#include "t30/nvboot_osc.h"
#include "nvrm_pmu.h"
#include "nvodm_query_discovery.h"
#include "nvrm_init.h"
#include "nvodm_query_gpio.h"


/* cycles corresponding to 13MHz, 16.8MHz, 19.2MHz, 38.4MHz, 12MHz, 48MHz, 26MHz */
static int fuse_pgm_cycles[] = {130, 168, 0, 0, 192, 384, 0, 0, 120, 480, 0, 0, 260};

// Map NvDdkSecBootDeviceType enum onto T30 Boot Device Selection values.
static NvDdkFuseBootDevice NvDdkToFuseBootDeviceSel[] =
{
    NvDdkFuseBootDevice_Sdmmc,         // None
    NvDdkFuseBootDevice_NandFlash,     // NvDdkSecBootDeviceType_Nand
    NvDdkFuseBootDevice_SnorFlash,     // NvDdkSecBootDeviceType_Nor
    NvDdkFuseBootDevice_SpiFlash,      // NvDdkSecBootDeviceType_Spi_Flash
    NvDdkFuseBootDevice_Sdmmc,         // NvDdkSecBootDeviceType_eMMC
    NvDdkFuseBootDevice_NandFlash,     // NvDdkSecBootDeviceType_Nand_x16
    NvDdkFuseBootDevice_MobileLbaNand, // NvDdkSecBootDeviceType_MobileLbaNand
    NvDdkFuseBootDevice_Sdmmc,         // NvDdkSecBootDeviceType_eMMC
    NvDdkFuseBootDevice_MuxOneNand,    // NvDdkSecBootDeviceType_MuxOneNand
    NvDdkFuseBootDevice_Sata,    // NvDdkSecBootDeviceType_Sata
    NvDdkFuseBootDevice_Sdmmc,    // NvDdkSecBootDeviceType_Sdmmc
};

static NvBootDevType MapFuseDevToBootDevType[] =
{
    NvBootDevType_Sdmmc,         // NvBootFuseBootDevice_Sdmmc
    NvBootDevType_Snor,          // NvBootFuseBootDevice_SnorFlash
    NvBootDevType_Spi,           // NvBootFuseBootDevice_SpiFlash
    NvBootDevType_Nand,          // NvBootFuseBootDevice_NandFlash
    NvBootDevType_MobileLbaNand, // NvBootFuseBootDevice_MobileLbaNand
    NvBootDevType_MuxOneNand,    // NvBootFuseBootDevice_MuxOneNand
    NvBootDevType_Sata,           // NvBootFuseBootDevice_Sata
    NvBootDevType_Sdmmc3,        // NvBootFuseBootDevice_BootRom_Reserved_Sdmmc3
};

NvU32 NvDdkToFuseBootDeviceSelMap(NvU32 RegData)
{
    if (RegData >= (int)((sizeof(NvDdkToFuseBootDeviceSel)) / sizeof(NvDdkFuseBootDevice)))
        return  -1;
    return NvDdkToFuseBootDeviceSel[RegData];
}

NvU32 NvFuseDevToBootDevTypeMap(NvU32 RegData)
{
    if (RegData >= (int)((sizeof(MapFuseDevToBootDevType)) / sizeof(NvBootDevType)))
        return  -1;
    return MapFuseDevToBootDevType[RegData];
}
NvError NvDdkFusePrivGetTypeSize(NvDdkFuseDataType Type,
                                 NvU32 *pSize)
{
    if (!pSize)
        return NvError_InvalidAddress;

    switch (Type)
    {
        case NvDdkFuseDataType_Uid:
            *pSize = NVDDK_UID_BYTES;
            break;

        case NvDdkFuseDataType_SecureBootKey:
            *pSize = NVDDK_SECURE_BOOT_KEY_BYTES;
            break;

        case NvDdkFuseDataType_ReservedOdm:
            *pSize = NVDDK_RESERVED_ODM_BYTES;
            break;

        default:
             return NvError_NotImplemented;
    }
    return NvSuccess;
}

void NvDdkFusePrivMapDataToFuseArrays(void)
{
    /* Same ODM fuses compared to Ap20 */
}

// Start the regulator and wait
void StartRegulator(void)
{
    NvU32 RegData;
    RegData = NV_DRF_DEF(FUSE, PWR_GOOD_SW, PWR_GOOD_SW_VAL, PWR_GOOD_OK);
    FUSE_NV_WRITE32(FUSE_PWR_GOOD_SW_0, RegData);

    /*
     * Wait for at least 150ns. In HDEFUSE_64X32_E2F2R2_L1_A.pdf,
     * this is found on p.11 as TSUP_PWRGD.
     */

    NvFuseUtilWaitUS(1);
}

// Stop the regulator and wait
void StopRegulator(void)
{
    NvU32 RegData;

    RegData = NV_DRF_DEF(FUSE, PWR_GOOD_SW, PWR_GOOD_SW_VAL, PWR_GOOD_NOT_OK);
    FUSE_NV_WRITE32(FUSE_PWR_GOOD_SW_0, RegData);

    /*
     * Wait for at least 150ns. In HDEFUSE_64X32_E2F2R2_L1_A.pdf,
     *  this is found on p.11 as TSUP_PWRGD
     */
    NvFuseUtilWaitUS(1);
}

NvU32 NvDdkFuseGetFusePgmCycles(void)
{
    NvBootClocksOscFreq freq;

    freq = NvBootClocksGetOscFreqT30();

    return fuse_pgm_cycles[(NvU32)freq];
}

NvError NvDdkFuseSetPriv(void** pData, NvDdkFuseDataType type,
                       NvDdkFuseData* p_FuseData, NvU32 Size)
{
    /* Same ODM fuses compared to Ap20 */

    return NvError_BadValue;
}

void NvDdkFuseFillChipOptions(void)
{
    NvU32 RegData;

    /* set PRIV2INTFC_START_DATA */
    RegData = NV_DRF_NUM(FUSE, PRIV2INTFC_START, PRIV2INTFC_START_DATA, 0x1);
    FUSE_NV_WRITE32(FUSE_PRIV2INTFC_START_0, RegData);

    /* set PRIV2INTFC_SKIP_RECORDS */
    RegData = NV_DRF_NUM(FUSE, PRIV2INTFC_START, PRIV2INTFC_SKIP_RAMREPAIR, 0x1);
    FUSE_NV_WRITE32(FUSE_PRIV2INTFC_START_0, RegData);
}
void EnableVppFuse(void)
{
    NvOdmServicesGpioHandle hGpio =  NULL;
    const NvOdmGpioPinInfo *gpioPinTable;
    NvOdmGpioPinHandle hFuseEnable;
    NvU32 gpioPinCount;

    hGpio = NvOdmGpioOpen();

    // get gpio Enable Fuse pin
    gpioPinTable = NvOdmQueryGpioPinMap(NvOdmGpioPinGroup_Fuse, 0,
                                                                &gpioPinCount);
    hFuseEnable = NvOdmGpioAcquirePinHandle(hGpio,
                              gpioPinTable[0].Port,
                              gpioPinTable[0].Pin);

    // configure Enable Fuse as output pin
    NvOdmGpioConfig(hGpio, hFuseEnable, NvOdmGpioPinMode_Output);

    // set Enable Fuse pin as high to enable VPP_FUSE power rail
    NvOdmGpioSetState(hGpio, hFuseEnable, 0x1);
}

void DisableVppFuse(void)
{
    NvOdmServicesGpioHandle hGpio =  NULL;
    const NvOdmGpioPinInfo *gpioPinTable;
    NvOdmGpioPinHandle hFuseEnable;
    NvU32 gpioPinCount;

    hGpio = NvOdmGpioOpen();

    // get gpio Enable Fuse pin
    gpioPinTable = NvOdmQueryGpioPinMap(NvOdmGpioPinGroup_Fuse, 0,
                                                                &gpioPinCount);
    hFuseEnable = NvOdmGpioAcquirePinHandle(hGpio,
                              gpioPinTable[0].Port,
                              gpioPinTable[0].Pin);

    // configure Enable Fuse as output pin
    NvOdmGpioConfig(hGpio, hFuseEnable, NvOdmGpioPinMode_Output);

    // set Enable Fuse pin as low to disable VPP_FUSE power rail
    NvOdmGpioSetState(hGpio, hFuseEnable, 0x0);
}

