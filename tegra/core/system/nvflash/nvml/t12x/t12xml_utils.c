/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#include "nvml_utils.h"
#include "nvml_device.h"
#include "nvboot_sdmmc_int.h"
#include "nvboot_snor_int.h"
#include "nvddk_fuse.h"
#include "nvddk_fuse.h"

#define MISC_PA_BASE 0x70000000UL
#define MISC_PA_LEN  4096
#define PMC_BASE    0x7000E400
#define CLK_RST_PA_BASE   0x60006000
#define PLLM_ENABLE_SHIFT 12

#define FUSE_RESERVED_SW_0_BOOT_DEVICE_SELECT_RANGE       2:0
#define FUSE_RESERVED_SW_0_SKIP_DEV_SEL_STRAPS_RANGE      3:3
#define FUSE_RESERVED_SW_0_SW_RESERVED_RANGE              7:4

extern NvBctHandle BctDeviceHandle;

/*
 * NvStrapDevSel: Enumerated list of devices selectable via straps.
 */
typedef enum
{
    NvStrapDevSel_Emmc_Primary_x4 = 0,              // eMMC primary (x4)
    NvStrapDevSel_Snor_Muxed_x16,                   // Sync NOR (Muxed, x16)
    NvStrapDevSel_Snor_Muxed_x32,                   // Sync NOR (Muxed, x32)
    NvStrapDevSel_Snor_NonMuxed_x16,                // Sync NOR (Non-Muxed, x16)
    NvStrapDevSel_Emmc_Primary_x8,                  // eMMC primary (x4)
    NvStrapDevSel_Emmc_x8_BootModeOff_29Mhz_8_Page, // eMMC x8 29 Mhz Multi 8 page
    NvStrapDevSel_Emmc_x8_BootModeOff_40Mhz_16_Page,// eMMC x8 29 Mhz Multi 16 page
    NvStrapDevSel_Snor_NonMuxed_x32,                // Sync NOR (Non-Muxed, x32)
    NvStrapDevSel_SpiFlash,                         // SPI Flash
    NvStrapDevSel_Reserved_0x9,                     // Exit RCM applies
    NvStrapDevSel_Emmc_BootModeOff_x4,              // eMMC x4 BootModeOFF
    NvStrapDevSel_Emmc_BootModeOff_x8,              // eMMC x8 BootModeOFF
    NvStrapDevSel_Emmc_BootModeOff_Div4_x8,         // eMMC x8 BootModeOff Clk Div4
    NvStrapDevSel_Reserved_0xD,                     // reserved
    NvStrapDevSel_Emmc_BootModeOff_Div6_x8,         // eMMC x8 BootModeOff Clk Div6
    NvStrapDevSel_UseFuses,                         // Use fuses instead

    /* The following definitions must be last. */
    NvStrapDevSel_Num, /* Must appear after the last legal item */
    NvStrapDevSel_Force32 = 0x7FFFFFFF
} NvStrapDevSel;

static NvBootFuseBootDevice StrapDevSelMap[] =
{
    NvBootFuseBootDevice_Sdmmc,         // NvBootStrapDevSel_Emmc_x4_BootModeOn
    NvBootFuseBootDevice_SnorFlash,     // NvBootStrapDevSel_Snor_Muxed_x16
    NvBootFuseBootDevice_SnorFlash,     // NvBootStrapDevSel_Snor_Muxed_x32
    NvBootFuseBootDevice_SnorFlash,     // NvBootStrapDevSel_Snor_NonMuxed_x16
    NvBootFuseBootDevice_Sdmmc,         // NvBootStrapDevSel_Emmc_x8_BootModeOn
    NvBootFuseBootDevice_Sdmmc,         // NvBootStrapDevSel_Emmc_x8_BootModeOff_ClkDivider_31Mhz_Multi_8_Page
    NvBootFuseBootDevice_Sdmmc,         // NvBootStrapDevSel_Emmc_x8_BootModeOff_clkDivider_45Mhz_Multi_16_Page
    NvBootFuseBootDevice_SnorFlash,     // NvBootStrapDevSel_Snor_NonMuxed_x32
    NvBootFuseBootDevice_SpiFlash,      // NvBootStrapDevSel_SpiFlash
    NvBootFuseBootDevice_resvd_3,       // Reserved; Default to RCM
    NvBootFuseBootDevice_Sdmmc,         // NvBootStrapDevSel_Emmc_BMOff_x4
    NvBootFuseBootDevice_Sdmmc,         // NvBootStrapDevSel_Emmc_BMOff_x8
    NvBootFuseBootDevice_Sdmmc,         // NvBootStrapDevSel_Emmc_BMOffDiv6_x8_BootModeOff
    NvBootFuseBootDevice_resvd_3,       // Reserved; Default to RCM
    NvBootFuseBootDevice_Sdmmc,         // NvBootStrapDevSel_Emmc_BMOff_Div6_x8
    NvBootFuseBootDevice_Sdmmc,         // NvBootStrapDdevSel_UseFuses
};

#define SDMMC_CONFIG(MultiPageSupport, ClockDivider, BusWidth, BootMode) \
    (((BusWidth) << 0) | ((BootMode) << 4) | ((ClockDivider) << 6) | \
    ((MultiPageSupport) << 10))

#define SNOR_CONFIG(NonMuxed, BusWidth) \
    (((BusWidth) << 0) | ((NonMuxed) << 1))

#define SPI_CONFIG(PageSize2kor16k) \
    ((PageSize2kor16k) << 0)

static NvU32 StrapDevConfigMap[] =
{
    SDMMC_CONFIG(0, 0, 0, 0),   // NvBootStrapDevSel_Emmc_x4_BootModeOn
    SNOR_CONFIG(0, 0),          // NvBootStrapDevSel_Snor_Muxed_x16
    SNOR_CONFIG(0, 1),          // NvBootStrapDevSel_Snor_Muxed_x32
    SNOR_CONFIG(1, 0),          // NvBootStrapDevSel_Snor_NonMuxed_x16
    SDMMC_CONFIG(0, 0, 1, 0),   // NvBootStrapDevSel_Emmc_x8_BootModeOn
    SDMMC_CONFIG(3, 4, 1, 1),   // NvBootStrapDevSel_Emmc_x8_BootModeOff_ClkDivider_31Mhz_Multi_8_Page
    SDMMC_CONFIG(4, 6, 1, 1),   // NvBootStrapDevSel_Emmc_x8_BootModeOff_clkDivider_45Mhz_Multi_16_Page
    SNOR_CONFIG(1, 1),          // NvBootStrapDevSel_Snor_NonMuxed_x32
    SPI_CONFIG(0),              // NvBootStrapDevSel_SpiFlash : Page size 2K
    0,                          // No config data
    SDMMC_CONFIG(0, 0, 0, 1),   // NvBootStrapDevSel_Emmc_BMOff_x4
    SDMMC_CONFIG(0, 0, 1, 1),   // NvBootStrapDevSel_Emmc_BMOff_x8
    SDMMC_CONFIG(0, 4, 1, 1),   // NvBootStrapDevSel_Emmc_BMOff_Div4_x8
    0,                          // No config data
    SDMMC_CONFIG(0, 6, 1, 1),   // NvBootStrapDevSel_Emmc_BMOff_Div6_x8
    0                           // No config data; NvBootStrapDevSel_UseFuses
};

void
NvMlHashBlocks (
    NvBootAesEngine engine,
    NvBootAesIv *pK1,
    NvU8 *pData,
    NvBootHash *pHash,
    NvU32 numBlocks,
    NvBool firstChunk,
    NvBool lastChunk) {}
void
NvMlHashGenerateSubkeys (
    NvBootAesEngine engine,
    NvBootAesIv *pK1) {}

NvBool
NvMlAesIsEngineBusy(NvBootAesEngine engine) { return NV_FALSE;}
void
NvMlAesEnableHashingInEngine(
    NvBootAesEngine engine,
    NvBool enbHashing) {}
NvBool
NvMlAesIsHashingEnabled(NvBootAesEngine engine) {return NV_FALSE;}
void
NvMlAesReadHashOutput(
    NvBootAesEngine engine,
    NvU8 *pHashResult) {}
void
NvMlAesSelectKeyIvSlot(
    NvBootAesEngine engine,
    NvBootAesKeySlot slot) {}
void
NvMlAesStartEngine(
    NvBootAesEngine engine,
    NvU32 nblocks,
    NvU8 *src,
    NvU8 *dst,
    NvBool IsEncryption) {}
void
NvMlAesInitializeEngine(void) {}
void
NvMlAesSetKeyAndIv(
    NvBootAesEngine engine,
    NvBootAesKeySlot slot,
    NvBootAesKey *pKey,
    NvBootAesIv *pIv,
    NvBool IsEncryption) {}
static NvError
NvMlUtilsConvertBootFuseDeviceToBootDevice(
    NvBootFuseBootDevice NvBootFuse,
    NvBootDevType *pBootDevice);

NvMlDevMgrCallbacks s_DeviceCallbacks[] =
{
    {
        /* Callbacks for the default (unset) device */
        (NvBootDeviceGetParams)NULL,
        (NvBootDeviceValidateParams)NULL,
        (NvBootDeviceGetBlockSizes)NULL,
        (NvBootDeviceInit)NULL,
        NULL,
        NULL,
        NULL,
    },
    {
        /* Callbacks for the NAND x8 device */
        (NvBootDeviceGetParams)NULL,
        (NvBootDeviceValidateParams)NULL,
        (NvBootDeviceGetBlockSizes)NULL,
        (NvBootDeviceInit)NULL,
        NULL,
        NULL,
        NULL,
    },
    {
        /* Callbacks for the SNOR device */
        (NvBootDeviceGetParams)NvBootSnorGetParams,
        (NvBootDeviceValidateParams)NvBootSnorValidateParams,
        (NvBootDeviceGetBlockSizes)NvBootSnorGetBlockSizes,
        (NvBootDeviceInit)NvBootSnorInit,
        NvBootSnorReadPage,
        NvBootSnorQueryStatus,
        NvBootSnorShutdown,
    },
    {
        /* Callbacks for the SPI Flash device */
        (NvBootDeviceGetParams)NvBootSpiFlashGetParams,
        (NvBootDeviceValidateParams)NvBootSpiFlashValidateParams,
        (NvBootDeviceGetBlockSizes)NvBootSpiFlashGetBlockSizes,
        (NvBootDeviceInit)NvBootSpiFlashInit,
        NvBootSpiFlashReadPage,
        NvBootSpiFlashQueryStatus,
        NvBootSpiFlashShutdown,
    },
    {
        /* Callbacks for the SDMMC  device */
        (NvBootDeviceGetParams)NvBootSdmmcGetParams,
        (NvBootDeviceValidateParams)NvBootSdmmcValidateParams,
        (NvBootDeviceGetBlockSizes)NvBootSdmmcGetBlockSizes,
        (NvBootDeviceInit)NvBootSdmmcInit,
        NvBootSdmmcReadPage,
        NvBootSdmmcQueryStatus,
        NvBootSdmmcShutdown,
    },
    {
        /* Callbacks for the Irom device */
        (NvBootDeviceGetParams)NULL,
        (NvBootDeviceValidateParams)NULL,
        (NvBootDeviceGetBlockSizes)NULL,
        (NvBootDeviceInit)NULL,
        NULL,
        NULL,
        NULL,
    },
    {
        /* Callbacks for the Uart device */
        (NvBootDeviceGetParams)NULL,
        (NvBootDeviceValidateParams)NULL,
        (NvBootDeviceGetBlockSizes)NULL,
        (NvBootDeviceInit)NULL,
        NULL,
        NULL,
        NULL,
    },
    {
        /* Callbacks for the Usb device */
        (NvBootDeviceGetParams)NULL,
        (NvBootDeviceValidateParams)NULL,
        (NvBootDeviceGetBlockSizes)NULL,
        (NvBootDeviceInit)NULL,
        NULL,
        NULL,
        NULL,
    },
    {
        /* Callbacks for the NAND x16 device */
        (NvBootDeviceGetParams)NULL,
        (NvBootDeviceValidateParams)NULL,
        (NvBootDeviceGetBlockSizes)NULL,
        (NvBootDeviceInit)NULL,
        NULL,
        NULL,
        NULL,
    },
    {
        /* Callbacks for the USB3 device */
        (NvBootDeviceGetParams)NULL,
        (NvBootDeviceValidateParams)NULL,
        (NvBootDeviceGetBlockSizes)NULL,
        (NvBootDeviceInit)NULL,
        NULL,
        NULL,
        NULL,
    },
};

NvBootError NvMlUtilsSetupBootDevice(void *Context)
{
    NvU32                ConfigIndex;
    NvBootFuseBootDevice FuseDev;
    NvMlContext* pContext = (NvMlContext*)Context;
    NvBootDevType BootDevType = NvBootDevType_None;
    NvBootError e = NvBootError_Success;

    /* Query device info from fuses. */
    NvMlGetBootDeviceType(&FuseDev);
    NvMlGetBootDeviceConfig(&ConfigIndex);
    /* no fusebypass  for t124 at miniloader .keeping the config index
    at safe boot fuse value */

    NvMlUtilsConvertBootFuseDeviceToBootDevice(FuseDev,&BootDevType);

    /* Initialize the device manager. */
    /* This is hanging for boot from usb usecase.
       TODO: it needs to be fixed */
        e = NvMlDevMgrInit(&(pContext->DevMgr),
                            BootDevType,
                            ConfigIndex);

    return e;
}

void
NvMlDoHashBlocks (
    NvBootAesEngine engine,
    NvBootAesIv *pK1,
    NvU8 *pData,
    NvBootHash *pHash,
    NvU32 numBlocks,
    NvBool startMsg,
    NvBool endMsg)
{
    // This implementation is not intended to be used when
    // hashing is not enabled. If hashing is not enabled in the
    // AES engine, hashing should be performed on 1 block at
    // a time.

    // Check if the engine is idle before performing an operation
    // Checking before would ensure that hashing operations are
    // non blocking AFAP.
    while ( NvMlAesIsEngineBusy(engine) )
        ;

    NvMlHashBlocks(engine,
                    pK1,
                    pData,
                    pHash,
                    numBlocks,
                    startMsg,
                    endMsg);


}

void
NvMlUtilsInitHash(NvBootAesEngine engine)
{
    NvMlAesEnableHashingInEngine(engine, NV_TRUE);
}

void
NvMlUtilsDeInitHash(NvBootAesEngine engine)
{
    NvMlAesEnableHashingInEngine(engine, NV_FALSE);
}

void
NvMlUtilsReadHashOutput(
    NvBootAesEngine engine,
    NvBootHash *pHash)
{
    if (NvMlAesIsHashingEnabled(engine))
    {
        // Check if engine is idle before reading the output
        while ( NvMlAesIsEngineBusy(engine) )
            ;
        NvMlAesReadHashOutput(engine, (NvU8 *)pHash);
    }
}

NvError
NvMlUtilsConvert3pToBootFuseDevice(
    Nv3pDeviceType DevNv3p,
    NvBootFuseBootDevice *pDevNvBootFuse)
{
    NvError errVal = NvError_Success;
    switch (DevNv3p)
    {
        // *FIXME*
        // case Nv3pDeviceType_Nand:
            // *pDevNvBootFuse = NvBootFuseBootDevice_NandFlash;
            // break;
        case Nv3pDeviceType_Snor:
            *pDevNvBootFuse = NvBootFuseBootDevice_SnorFlash;
            break;
        case Nv3pDeviceType_Spi:
            *pDevNvBootFuse = NvBootFuseBootDevice_SpiFlash;
            break;
        case Nv3pDeviceType_Emmc:
            *pDevNvBootFuse = NvBootFuseBootDevice_Sdmmc;
            break;
        // *FIXME*
        // case Nv3pDeviceType_Usb3:
            // *pDevNvBootFuse = NvBootFuseBootDevice_Usb3;
            // break;
        default:
            *pDevNvBootFuse = NvBootFuseBootDevice_Max;
            errVal = NvError_BadParameter;
            break;
    }
    return errVal;
}
NvError
NvMlUtilsConvertBootFuseDeviceTo3p(
    NvBootFuseBootDevice NvBootFuse,
    Nv3pDeviceType *pNv3pDevice)
{
    NvError errVal = NvError_Success;
    switch (NvBootFuse)
    {
        // *FIXME*
        // case NvBootFuseBootDevice_NandFlash:
            // *pNv3pDevice = Nv3pDeviceType_Nand;
            // break;
        case NvBootFuseBootDevice_SnorFlash:
            *pNv3pDevice = Nv3pDeviceType_Snor;
            break;
        case NvBootFuseBootDevice_Sdmmc:
            *pNv3pDevice = Nv3pDeviceType_Emmc;
            break;
        case NvBootFuseBootDevice_SpiFlash:
            *pNv3pDevice = Nv3pDeviceType_Spi;
            break;
        // *FIXME*
        // case NvBootFuseBootDevice_Usb3:
            // *pNv3pDevice = Nv3pDeviceType_Usb3;
            // break;
        default:
            *pNv3pDevice = Nv3pDeviceType_Force32;
            errVal = NvError_BadParameter;
            break;
    }
    return errVal;
}

NvError
NvMlUtilsConvertBootFuseDeviceToBootDevice(
    NvBootFuseBootDevice NvBootFuse,
    NvBootDevType *pBootDevice)
{
    NvError errVal = NvError_Success;
    switch (NvBootFuse)
    {
        // *FIXME* NAND not POR.
        // case NvBootFuseBootDevice_NandFlash:
            // *pBootDevice = NvBootDevType_Nand;
            // break;
        case NvBootFuseBootDevice_Sdmmc:
            *pBootDevice = NvBootDevType_Sdmmc;
            break;
        case NvBootFuseBootDevice_SpiFlash:
            *pBootDevice = NvBootDevType_Spi;
        case NvBootFuseBootDevice_SnorFlash:
            *pBootDevice = NvBootDevType_Snor;
            break;
        // *FIXME* USB3 not POR.
        // case NvBootFuseBootDevice_Usb3:
            // *pBootDevice = NvBootDevType_Usb3;
            // break;
        default:
            *pBootDevice = NvBootDevType_Force32;
            errVal = NvError_BadParameter;
            break;
    }
    return errVal;
}
NvBootDevType
NvMlUtilsGetSecondaryBootDevice(void)
{
    NvBootDevType        DevType;
    NvBootFuseBootDevice FuseDev;
    /* Query device info from fuses. */
    NvMlGetBootDeviceType(&FuseDev);

    switch( FuseDev )
    {
        case NvBootFuseBootDevice_Sdmmc:
            DevType = NvBootDevType_Sdmmc;
            break;
        case NvBootFuseBootDevice_SnorFlash:
            DevType = NvBootDevType_Snor;
            break;
        case NvBootFuseBootDevice_SpiFlash:
            DevType = NvBootDevType_Spi;
            break;
// *FIXME* NAND not POR
        // case NvBootFuseBootDevice_NandFlash:
            // DevType = NvBootDevType_Nand;
            // break;
// *FIXME* USB3 not POR
        // case NvBootFuseBootDevice_Usb3:
            // DevType = NvBootDevType_Usb3;
            // break;
        default:
            DevType = NvBootDevType_None;
            break;
    }
    return DevType;
}

static NvBool NvFuseSkipDevSelStraps(void)
{
    NvU32 RegData;

    RegData = NV_READ32(NV_ADDRESS_MAP_FUSE_BASE + FUSE_RESERVED_SW_0);
    RegData = NV_DRF_VAL(FUSE, RESERVED_SW, SKIP_DEV_SEL_STRAPS, RegData);

    if (RegData)
        return NV_TRUE;
    else
        return NV_FALSE;
}


static NvStrapDevSel NvStrapDeviceSelection(void)
{
     NvU32 RegData;
    NvU8 *pMisc;
    NvError e;

    NV_CHECK_ERROR_CLEANUP(
        NvOsPhysicalMemMap(PMC_BASE, MISC_PA_LEN,
            NvOsMemAttribute_Uncached, NVOS_MEM_READ_WRITE, (void **)&pMisc));

    RegData = NV_READ32(pMisc + APBDEV_PMC_STRAPPING_OPT_A_0);

    NvOsPhysicalMemUnmap(pMisc, MISC_PA_LEN);

    return (NvStrapDevSel)(NV_DRF_VAL(APBDEV_PMC,
                                          STRAPPING_OPT_A,
                                          BOOT_SELECT,
                                          RegData));

fail:
    return NvStrapDevSel_Emmc_Primary_x4;
}

void NvMlUtilsGetBootDeviceType(NvBootFuseBootDevice *pDevType)
{

    NvStrapDevSel StrapDevSel;
    NvU32 Size;

#ifdef BOOT_MINIMAL_BL
    *pDevType = NvBootFuseBootDevice_Sdmmc;
#else
    // Read device selcted using the strap
    StrapDevSel = NvStrapDeviceSelection();

   /* Determine if the fuse data should be overwritten by strap data. */
    if (!NvFuseSkipDevSelStraps() && StrapDevSel != NvStrapDevSel_UseFuses)
    {
        /* Handle out-of-bound values */
        if (StrapDevSel >= NvStrapDevSel_Num)
            StrapDevSel = NvStrapDevSel_Emmc_Primary_x4;

        *pDevType  = StrapDevSelMap[StrapDevSel];
    }
    else
    {
        // Read from fuse
        Size = sizeof(NvU32);
        NvDdkFuseGet(NvDdkFuseDataType_SecBootDeviceSelectRaw,
                    (void *)pDevType, (NvU32 *)&Size);
    }
    if (*pDevType >= NvBootFuseBootDevice_Max)
        *pDevType = NvBootFuseBootDevice_Max;
#endif
}

void NvMlUtilsGetBootDeviceConfig(NvU32 *pConfigVal)
{
    NvStrapDevSel StrapDevSel;
    NvU32 BootConfigSize;

#ifdef BOOT_MINIMAL_BL
    *pConfigVal = 0x11;
#else
    // Read device selcted using the strap
    StrapDevSel = NvStrapDeviceSelection();

    /* Determine if the fuse data should be overwritten by strap data. */
    if (!NvFuseSkipDevSelStraps() && StrapDevSel != NvStrapDevSel_UseFuses)
    {
        /* Handle out-of-bound values */
        if (StrapDevSel >= NvStrapDevSel_Num)
            StrapDevSel = NvStrapDevSel_Emmc_Primary_x4;

        *pConfigVal = StrapDevConfigMap[StrapDevSel];
    }
    else
    {
        // Read from fuse
        BootConfigSize = sizeof (NvU32);
        // Get BootConfig data
        NvDdkFuseGet(NvDdkFuseDataType_SecBootDeviceConfig,
                    (void *)pConfigVal, (NvU32 *)&BootConfigSize);
    }
#endif
}

NvError NvMlUtilsValidateBctDevType()
{
    NvBootFuseBootDevice FuseDev;
    NvBootDevType BootDev;
    NvU32 Size = sizeof(NvBootDevType);
    NvU32 Instance = 0;
    NvError e = NvError_BadParameter;

    NV_CHECK_ERROR_CLEANUP(
        NvBctGetData(BctDeviceHandle, NvBctDataType_DevType,
                     &Size, &Instance, &BootDev)
    );

    NvMlUtilsGetBootDeviceType(&FuseDev);
    if (FuseDev != NvBootFuseBootDevice_Max)
    {
        if (FuseDev == BootDev)
        {
            e = NvSuccess;
        }
    }

fail:
    return e;
}

NvU32 NvMlUtilsGetBctSize(void)
{
    return sizeof(NvBootConfigTable);
}

// This implementation is required only in T11x since there is no
//PLLM auto restart mechanism on ap15/20
void SetPllmInRecovery(void)
{
    NvU32 regClockBase;
    NvU32 reg;
    NvU32 Divn, Divm;

    regClockBase = NV_READ32(CLK_RST_PA_BASE + CLK_RST_CONTROLLER_PLLM_BASE_0);

    //PLLM_WB0 values are copied from CLK_RST_CONTROLLER_PLLM_BASE_0
    //and CLK_RST_CONTROLLER_PLLM_MISC_0
    Divm = NV_DRF_VAL(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_DIVM, regClockBase);
    Divn = NV_DRF_VAL(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_DIVN, regClockBase);

    reg = NV_DRF_NUM(APBDEV_PMC, PLLM_WB0_OVERRIDE_FREQ, PLLM_DIVM, Divm)
        | NV_DRF_NUM(APBDEV_PMC, PLLM_WB0_OVERRIDE_FREQ, PLLM_DIVN, Divn);

    NV_WRITE32(PMC_BASE + APBDEV_PMC_PLLM_WB0_OVERRIDE_FREQ_0, reg);

    //PLLM_ENABLE
    reg = NV_READ32(PMC_BASE + APBDEV_PMC_PLLP_WB0_OVERRIDE_0);
    reg = reg | (1 << PLLM_ENABLE_SHIFT);
    NV_WRITE32(PMC_BASE + APBDEV_PMC_PLLP_WB0_OVERRIDE_0, reg);
}

void NvMlUtilsFuseGetUniqueId(void * Id)
{
    NvU32 UniqueIdSize = sizeof(Nv3pChipUniqueId);

    //Get Unique Id
    NvDdkFuseGet(NvDdkFuseDataType_Uid, (void *)Id, (NvU32 *)&UniqueIdSize);
}

void
NvMlUtilsSskGenerate(NvBootAesEngine SbkEngine,
    NvBootAesKeySlot SbkEncryptSlot,
    NvU8 *ssk)
{
    const NvU32 BlockLengthBytes = NVBOOT_AES_BLOCK_LENGTH_BYTES;
    NvU8 uid[NVBOOT_AES_BLOCK_LENGTH_BYTES];
    NvU8 dk[NVBOOT_AES_BLOCK_LENGTH_BYTES];
    NvU32 i;
    NvU32 UidSize;

    // SSK is calculated as follows --
    //
    // SSK = AES(SBK; UID ^ AES(SBK; DK; Encrypt); Encrypt)

    // We start with an assumption that DK is zero
    NvOsMemset(dk, 0, NVBOOT_AES_BLOCK_LENGTH_BYTES);

    // copy the 4 byte device key into the consecutive word locations
    // to frame the 16 byte AES block;
    for (i = 0; i < 4; i++)
    {
        dk[i+4] = dk[i+8] = dk[i+12] = dk[i];
    }

    // Make sure engine is idle, then select SBK encryption key.  Encrypt
    // Device Key with the SBK, storing the result in dk.  Finally, wait
    // for engine to become idle.

    while (NvMlAesIsEngineBusy(SbkEngine));
    NvMlAesSelectKeyIvSlot(SbkEngine, SbkEncryptSlot);
    NvMlAesStartEngine(SbkEngine, 1 /*1 block*/, dk, dk, NV_TRUE);
    while (NvMlAesIsEngineBusy(SbkEngine));

    // Get Unique ID from fuses (NvBootECID is 128-bits), so no padding needed.
    //Get the size of Uid
    UidSize = sizeof(Nv3pChipUniqueId);
    // Get Uid
    NvDdkFuseGet(NvDdkFuseDataType_Uid, (void *)&uid, (NvU32 *)&UidSize);

    // XOR the uid and AES(SBK, DK, Encrypt)
    for (i = 0; i < BlockLengthBytes; i++)
        ssk[i] = uid[i] ^ dk[i];

    // Make sure engine is idle, then select SBK encryption key.  Compute
    // the final SSK value by perform a second SBK encryption operation,
    // and store the result in sskdk.  Wait for engine to become idle.
    //
    // Note: re-selecting the SBK key slot is needed in order to reset the
    //       IV to the pre-set value

    while (NvMlAesIsEngineBusy(SbkEngine));
    NvMlAesSelectKeyIvSlot(SbkEngine, SbkEncryptSlot);
    NvMlAesStartEngine(SbkEngine, 1 /*1 block*/, ssk, ssk, NV_TRUE);
    while (NvMlAesIsEngineBusy(SbkEngine));

    // Re-select slot to clear any left-over state information
    NvMlAesSelectKeyIvSlot(SbkEngine, SbkEncryptSlot);
}

void
NvMlUtilsClocksStartPll(NvU32 PllSet,
    NvBootSdramParams *pData,
    NvU32 StableTime)
{
    NvU32 Misc1;
    NvU32 Misc2;

    //Pack PLLM params into Misc1 and Misc1
    Misc1 = NV_DRF_NUM(MISC1, CLK_RST_CONTROLLER_PLLM_MISC1,
                    PLLM_SETUP, pData->PllMSetupControl) | \
                    NV_DRF_NUM(MISC1, CLK_RST_CONTROLLER_PLLM_MISC1,
                    PLLM_PD_LSHIFT_PH45, pData->PllMPDLshiftPh45) | \
                    NV_DRF_NUM(MISC1, CLK_RST_CONTROLLER_PLLM_MISC1,
                    PLLM_PD_LSHIFT_PH90, pData->PllMPDLshiftPh90) | \
                    NV_DRF_NUM(MISC1, CLK_RST_CONTROLLER_PLLM_MISC1,
                    PLLM_PD_LSHIFT_PH135,pData->PllMPDLshiftPh135);

    Misc2 = NV_DRF_NUM(MISC2, CLK_RST_CONTROLLER_PLLM_MISC2, PLLM_KVCO,
                    pData->PllMKVCO) | \
                    NV_DRF_NUM(MISC2, CLK_RST_CONTROLLER_PLLM_MISC2,
                    PLLM_KCP, pData->PllMKCP);

    NvBootClocksStartPll(PllSet,
                         pData->PllMInputDivider,           // M
                         pData->PllMFeedbackDivider,        // N
                         pData->PllMSelectDiv2,             // P
                         Misc1,
                         Misc2,
                         &StableTime);
    while (!(NvBootClocksIsPllStable(PllSet, StableTime)));
    NvOsWaitUS(100000);
}


NvU32 NvMlUtilsGetPLLPFreq(void)
{
    return PLLP_FIXED_FREQ_KHZ_408000;
}

#if NO_BOOTROM

void NvMlUtilsPreInitBit(NvBootInfoTable *pBitTable)
{
    NvOsMemset( (void*) pBitTable, 0, sizeof(NvBootInfoTable) ) ;

    pBitTable->BootRomVersion = NVBOOT_VERSION(0x40,0x01);
    pBitTable->DataVersion = NVBOOT_VERSION(0x40,0x01);
    pBitTable->RcmVersion = NVBOOT_VERSION(0x40,0x01);
    pBitTable->BootType = NvBootType_Recovery;
    pBitTable->PrimaryDevice = NvBootDevType_Irom;

    pBitTable->OscFrequency = NvBootClocksGetOscFreq();
    // Workaround for missing BIT in Boot ROM version 1.000
    //
    // Rev 1.000 of the Boot ROM incorrectly clears the BIT when a Forced
    // Recovery is performed.  Fortunately, very few fields of the BIT are
    // set as a result of a Forced Recovery, so most everything can be
    // reconstructed after the fact.
    //
    // One piece of information that is lost is the source of the
    // Forced Recovery -- by strap or by flag (PMC Scratch0).  If the
    // flag was set, then it is cleared and the ClearedForceRecovery
    // field is set to NV_TRUE.  Since the BIT has been cleared by
    // time an applet is allowed to execute, there's no way to
    // determine if the flag in PMC Scratch0 has been cleared.  If the
    // strap is not set, though, then one can infer that the flag must
    // have been set (and subsequently cleared).  However, if the
    // strap is not set, there's no way to know whether or not the
    // flag was also set.

    // TODO -- check Force Recovery strap to partially infer whether PMC
    //         Scratch0 flag was cleared (see note above for details)

    pBitTable->ClearedForceRecovery = NV_FALSE;
    pBitTable->SafeStartAddr = (NvU32)pBitTable + sizeof(NvBootInfoTable);

    pBitTable->DevInitialized = NV_TRUE;

}

NvBool NvMlUtilsStartUsbEnumeration()
{
    //BR uses address space from 0x40000000 to
    //0x4000E000.this code comes in to picture
    //if there is no BR binary.
    NvU8 *UsbBuffer = (NvU8 *)0x4000A000;

    //initialize usb driver context
    NvBootUsbfInit();

    for ( ; ; )
    {
        // Look for cable connection
        if (NvBootUsbfIsCableConnected())
        {
            // Start enumeration
            if (NvBootUsbfStartEnumeration(UsbBuffer) == NvBootError_Success)
            {
                // Success fully enumerated
                return NV_TRUE;
            }
            else
            {
                // Failed to enumerate.
                return NV_FALSE;
            }
        }
    }
}

#endif

