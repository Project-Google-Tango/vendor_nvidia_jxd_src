/*
 * Copyright (c) 2009-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvml_utils.h"
#include "nvml_device.h"
#include "nvboot_sdmmc_int.h"
#include "nvboot_snor_int.h"
#include "t30/arfuse.h"
#include "t30/arclk_rst.h"
#include "t30/arapbpm.h"
#include "nvml_aes.h"
#include "nvml_hash.h"
#include "nvddk_fuse.h"

#define MISC_PA_BASE 0x70000000UL
#define MISC_PA_LEN  4096
#define T30_PMC_BASE    0x7000E400
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
    NvStrapDevSel_Emmc_Primary_x4 = 0, /* eMMC primary (x4)        */
    NvStrapDevSel_Emmc_Primary_x8,     /* eMMC primary (x8)        */
    NvStrapDevSel_Emmc_Secondary_x4,   /* eMMC secondary (x4)      */
    NvStrapDevSel_Nand,                /* NAND (x8 or x16)         */
    NvStrapDevSel_Nand_42nm,           /* NAND (x8 or x16)         */
    NvStrapDevSel_MobileLbaNand,       /* mobileLBA NAND           */
    NvStrapDevSel_MuxOneNand,          /* MuxOneNAND               */
    NvStrapDevSel_Nand_42nm_meo_2,     /* NAND (x8 or x16)         */
    NvStrapDevSel_SpiFlash,            /* SPI Flash                */
    NvStrapDevSel_Snor_Muxed_x16,      /* Sync NOR (Muxed, x16)    */
    NvStrapDevSel_Snor_Muxed_x32,      /* Sync NOR (Muxed, x32)    */
    NvStrapDevSel_Snor_NonMuxed_x16,   /* Sync NOR (NonMuxed, x16) */
    NvStrapDevSel_FlexMuxOneNand,      /* FlexMuxOneNAND           */
    NvStrapDevSel_Sata,                /* Sata                     */
    NvStrapDevSel_Emmc_Secondary_x8,   /* eMMC secondary (x4)      */
    NvStrapDevSel_UseFuses,            /* Use fuses instead        */

    /* The following definitions must be last. */
    NvStrapDevSel_Num, /* Must appear after the last legal item */
    NvStrapDevSel_Force32 = 0x7FFFFFFF
} NvStrapDevSel;


static NvBootFuseBootDevice StrapDevSelMap[] =
{
    NvBootFuseBootDevice_Sdmmc,         // NvStrapDevSel_Emmc_Primary_x4
    NvBootFuseBootDevice_Sdmmc,         // NvStrapDevSel_Emmc_Primary_x8
    NvBootFuseBootDevice_Sdmmc,         // NvStrapDevSel_Emmc_Secondary_x4
    NvBootFuseBootDevice_NandFlash,     // NvStrapDevSel_Nand
    NvBootFuseBootDevice_NandFlash,     // NvStrapDevSel_Nand_42nm
    NvBootFuseBootDevice_MobileLbaNand, // NvStrapDevSel_MobileLbaNand
    NvBootFuseBootDevice_MuxOneNand,    // NvStrapDevSel_MuxOneNand
    NvBootFuseBootDevice_NandFlash,     // NvStrapDevSel_Nand
    NvBootFuseBootDevice_SpiFlash,      // NvStrapDevSel_SpiFlash
    NvBootFuseBootDevice_SnorFlash,     // NvStrapDevSel_Snor_Muxed_x16
    NvBootFuseBootDevice_SnorFlash,     // NvStrapDevSel_Snor_Muxed_x32
    NvBootFuseBootDevice_SnorFlash,     // NvStrapDevSel_Snor_NonMuxed_x16
    NvBootFuseBootDevice_MuxOneNand,    // NvStrapDevSel_FlexMuxOneNand
    NvBootFuseBootDevice_Sata,          // NvStrapDevSel_Sata
    NvBootFuseBootDevice_Sdmmc,         // NvStrapDevSel_Reserved_0xe
    NvBootFuseBootDevice_Sdmmc,         // NvStrapDevSel_UseFuses
};

#define NAND_CONFIG(DataWidth, PageOffset, BlockOffset, MainEccOffset_2, ToggleDDR) \
    (((DataWidth) << 3) | ((PageOffset) << 6) | ((BlockOffset) << 7) | \
    ((MainEccOffset_2) << 8) |((ToggleDDR) << 12))


#define SDMMC_CONFIG(BusWidth, SdMmcSel, PinMux) \
    (((BusWidth) << 0) | ((SdMmcSel) << 1) | ((PinMux) << 5))

#define SNOR_CONFIG(NonMuxed, BusWidth) \
    (((BusWidth) << 0) | ((NonMuxed) << 1))

static NvU32 StrapDevConfigMap[] =
{
    SDMMC_CONFIG(0, 0, 0),      // NvBootStrapDevSel_Emmc_Primary_x4
    SDMMC_CONFIG(1, 0, 0),      // NvBootStrapDevSel_Emmc_Primary_x8
    SDMMC_CONFIG(0, 0, 1),      // NvBootStrapDevSel_Emmc_Secondary_x4
    NAND_CONFIG(0, 0, 0, 0, 1), // NvBootStrapDevSel_Nand
    NAND_CONFIG(1, 1, 1, 0, 0), // NvBootStrapDevSel_Nand_42nm
    0, // No config data        // NvBootStrapDevSel_MobileLbaNand
    65, // MuxOneNand           // NvBootStrapDevSel_MuxOneNand
    NAND_CONFIG(1, 1, 1, 2, 0), // NvBootStrapDevSel_Nand
    0, // No config data       // NvBootStrapDevSel_SpiFlash
    SNOR_CONFIG(0, 0),         // NvBootStrapDevSel_Snor_Muxed_x16
    SNOR_CONFIG(0, 1),         // NvBootStrapDevSel_Snor_Muxed_x32
    SNOR_CONFIG(1, 0),         // NvBootStrapDevSel_Snor_NonMuxed_x16
    0, // FlexMuxOneNand       // NvBootStrapDevSel_FlexMuxOneNand
    0, // No config data       // NvBootStrapDevSel_Sata
    SDMMC_CONFIG(1, 0, 1),     // No config data   // NvBootStrapDevSel_Reserved_0xe
    0 // No config data        // NvBootStrapDevSel_UseFuses
};


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
        (NvBootDeviceGetParams)NvBootNandGetParams,
        (NvBootDeviceValidateParams)NvBootNandValidateParams,
        (NvBootDeviceGetBlockSizes)NvBootNandGetBlockSizes,
        (NvBootDeviceInit)NvBootNandInit,
        NvBootNandReadPage,
        NvBootNandQueryStatus,
        NvBootNandShutdown,
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
        (NvBootDeviceGetParams)NvBootNandGetParams,
        (NvBootDeviceValidateParams)NvBootNandValidateParams,
        (NvBootDeviceGetBlockSizes)NvBootNandGetBlockSizes,
        (NvBootDeviceInit)NvBootNandInit,
        NvBootNandReadPage,
        NvBootNandQueryStatus,
        NvBootNandShutdown,
    },
    {
        /* Callbacks for the MuxOnenand device */
        (NvBootDeviceGetParams)NULL,
        (NvBootDeviceValidateParams)NULL,
        (NvBootDeviceGetBlockSizes)NULL,
        (NvBootDeviceInit)NULL,
        NULL,
        NULL,
        NULL,
    },
    {
        /* Callbacks for the MobileLbaNand device */
        (NvBootDeviceGetParams)NULL,
        (NvBootDeviceValidateParams)NULL,
        (NvBootDeviceGetBlockSizes)NULL,
        (NvBootDeviceInit)NULL,
        NULL,
        NULL,
        NULL,
    },
    {
        /* Callbacks for the Sata device */
        (NvBootDeviceGetParams)NvBootSataGetParams,
        (NvBootDeviceValidateParams)NvBootSataValidateParams,
        (NvBootDeviceGetBlockSizes)NvBootSataGetBlockSizes,
        (NvBootDeviceInit)NvBootSataInit,
        NvBootSataReadPage,
        NvBootSataQueryStatus,
        NvBootSataShutdown,
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
    NvMlUtilsConvertBootFuseDeviceToBootDevice(FuseDev,&BootDevType);

    /* Initialize the device manager. */
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
        case Nv3pDeviceType_Nand:
            *pDevNvBootFuse = NvBootFuseBootDevice_NandFlash;
            break;
        case Nv3pDeviceType_Snor:
            *pDevNvBootFuse = NvBootFuseBootDevice_SnorFlash;
            break;
        case Nv3pDeviceType_Spi:
            *pDevNvBootFuse = NvBootFuseBootDevice_SpiFlash;
            break;
        case Nv3pDeviceType_Nand_x16:
            *pDevNvBootFuse = NvBootFuseBootDevice_NandFlash_x16;
            break;
        case Nv3pDeviceType_Emmc:
            *pDevNvBootFuse = NvBootFuseBootDevice_Sdmmc;
            break;
        case Nv3pDeviceType_MuxOneNand:
            *pDevNvBootFuse = NvBootFuseBootDevice_MuxOneNand;
            break;
        case Nv3pDeviceType_MobileLbaNand:
            *pDevNvBootFuse = NvBootFuseBootDevice_MobileLbaNand;
            break;
        case Nv3pDeviceType_Sata:
            *pDevNvBootFuse = NvBootFuseBootDevice_Sata;
            break;
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
        case NvBootFuseBootDevice_NandFlash:
            *pNv3pDevice = Nv3pDeviceType_Nand;
            break;
        case NvBootFuseBootDevice_SnorFlash:
            *pNv3pDevice = Nv3pDeviceType_Snor;
            break;
        case NvBootFuseBootDevice_Sdmmc:
            *pNv3pDevice = Nv3pDeviceType_Emmc;
            break;
        case NvBootFuseBootDevice_SpiFlash:
            *pNv3pDevice = Nv3pDeviceType_Spi;
            break;
        case NvBootFuseBootDevice_MuxOneNand:
            *pNv3pDevice = Nv3pDeviceType_MuxOneNand;
            break;
        case NvBootFuseBootDevice_MobileLbaNand:
            *pNv3pDevice = Nv3pDeviceType_MobileLbaNand;
            break;
        case NvBootFuseBootDevice_Sata:
            *pNv3pDevice = Nv3pDeviceType_Sata;
            break;
        default:
            *pNv3pDevice = Nv3pDeviceType_Force32;
            errVal = NvError_BadParameter;
            break;
    }
    return errVal;
}

NvError
NvMlUtilsConvertBootDevTypeTo3p(
        NvBootDevType BootDev,
        Nv3pDeviceType *pNv3pDevice)
{
    NvError errVal = NvError_Success;
    switch (BootDev)
    {
         case NvBootDevType_Sdmmc:
             *pNv3pDevice = Nv3pDeviceType_Emmc;
             break;
         case NvBootDevType_Snor:
             *pNv3pDevice = Nv3pDeviceType_Snor;
             break;
         case NvBootDevType_Nand:
             *pNv3pDevice = Nv3pDeviceType_Nand;
             break;
         case NvBootDevType_Spi:
             *pNv3pDevice = Nv3pDeviceType_Spi;
             break;
         case NvBootDevType_MuxOneNand:
             *pNv3pDevice = Nv3pDeviceType_MuxOneNand;
             break;
         case NvBootDevType_MobileLbaNand:
             *pNv3pDevice = Nv3pDeviceType_MobileLbaNand;
             break;
         case NvBootDevType_Sata:
             *pNv3pDevice = Nv3pDeviceType_Sata;
             break;
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
        case NvBootFuseBootDevice_NandFlash:
            *pBootDevice = NvBootDevType_Nand;
            break;
        case NvBootFuseBootDevice_Sdmmc:
            *pBootDevice = NvBootDevType_Sdmmc;
            break;
        case NvBootFuseBootDevice_SpiFlash:
            *pBootDevice = NvBootDevType_Spi;
            break;
        case NvBootFuseBootDevice_MuxOneNand:
            *pBootDevice = NvBootDevType_MuxOneNand;
            break;
        case NvBootFuseBootDevice_MobileLbaNand:
            *pBootDevice = NvBootDevType_MobileLbaNand;
            break;
        case NvBootFuseBootDevice_SnorFlash:
            *pBootDevice = NvBootDevType_Snor;
            break;
        case NvBootFuseBootDevice_Sata:
            *pBootDevice = NvBootDevType_Sata;
            break;
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
        case NvBootFuseBootDevice_NandFlash:
            DevType = NvBootDevType_Nand;
            break;
        case NvBootFuseBootDevice_MobileLbaNand:
            DevType = NvBootDevType_MobileLbaNand;
            break;
        case NvBootFuseBootDevice_MuxOneNand:
            DevType = NvBootDevType_MuxOneNand;
            break;
        case NvBootFuseBootDevice_Sata:
            DevType = NvBootDevType_Sata;
            break;
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
        NvOsPhysicalMemMap(MISC_PA_BASE, MISC_PA_LEN,
            NvOsMemAttribute_Uncached, NVOS_MEM_READ_WRITE, (void **)&pMisc));

    RegData = NV_READ32(pMisc + APB_MISC_PP_STRAPPING_OPT_A_0);

    NvOsPhysicalMemUnmap(pMisc, MISC_PA_LEN);

    return (NvStrapDevSel)(NV_DRF_VAL(APB_MISC_PP,
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
}

void NvMlUtilsGetBootDeviceConfig(NvU32 *pConfigVal)
{
    NvStrapDevSel StrapDevSel;
    NvU32 BootConfigSize;

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

// This implementation is required only in T30 since there is no
//PLLM auto restart mechanism on ap15/20
void SetPllmInRecovery(void)
{
    NvU32 regClockBase,regClockMisc;
    NvU32 reg;
    NvU32 Dcon, Cpcon, Lfcon, Vcocon, Divp, Divn, Divm;

    regClockBase = NV_READ32(CLK_RST_PA_BASE + CLK_RST_CONTROLLER_PLLM_BASE_0);
    regClockMisc = NV_READ32(CLK_RST_PA_BASE + CLK_RST_CONTROLLER_PLLM_MISC_0);
    //PLLM_WB0 values are copied from CLK_RST_CONTROLLER_PLLM_BASE_0
    //and CLK_RST_CONTROLLER_PLLM_MISC_0
    Divm = NV_DRF_VAL(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_DIVM, regClockBase);
    Divn = NV_DRF_VAL(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_DIVN, regClockBase);
    Divp = NV_DRF_VAL(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_DIVP, regClockBase);
    Vcocon = NV_DRF_VAL(CLK_RST_CONTROLLER, PLLM_MISC, PLLM_VCOCON, regClockMisc);
    Lfcon = NV_DRF_VAL(CLK_RST_CONTROLLER, PLLM_MISC, PLLM_LFCON, regClockMisc);
    Cpcon = NV_DRF_VAL(CLK_RST_CONTROLLER, PLLM_MISC, PLLM_CPCON, regClockMisc);
    Dcon = NV_DRF_VAL(CLK_RST_CONTROLLER, PLLM_MISC, PLLM_DCCON, regClockMisc);

    reg = NV_DRF_NUM(APBDEV_PMC, PLLM_WB0_OVERRIDE_FREQ, PLLM_DIVM, Divm)
        | NV_DRF_NUM(APBDEV_PMC, PLLM_WB0_OVERRIDE_FREQ, PLLM_DIVN, Divn)
        | NV_DRF_NUM(APBDEV_PMC, PLLM_WB0_OVERRIDE_FREQ, PLLM_DIVP, Divp)
        | NV_DRF_NUM(APBDEV_PMC, PLLM_WB0_OVERRIDE_FREQ, PLLM_VCOCON, Vcocon)
        | NV_DRF_NUM(APBDEV_PMC, PLLM_WB0_OVERRIDE_FREQ, PLLM_LFCON, Lfcon)
        | NV_DRF_NUM(APBDEV_PMC, PLLM_WB0_OVERRIDE_FREQ, PLLM_CPCON, Cpcon)
        | NV_DRF_NUM(APBDEV_PMC, PLLM_WB0_OVERRIDE_FREQ, PLLM_DCCON, Dcon);

    NV_WRITE32(T30_PMC_BASE + APBDEV_PMC_PLLM_WB0_OVERRIDE_FREQ_0, reg);

    //PLLM_ENABLE
    reg = NV_READ32(T30_PMC_BASE + APBDEV_PMC_PLLP_WB0_OVERRIDE_0);
    reg = reg | (1 << PLLM_ENABLE_SHIFT);
    NV_WRITE32(T30_PMC_BASE + APBDEV_PMC_PLLP_WB0_OVERRIDE_0, reg);
}

void
NvMlUtilsSskGenerate(NvBootAesEngine SbkEngine,
    NvBootAesKeySlot SbkEncryptSlot,
    NvU8 *ssk)
{
    const NvU32 BlockLengthBytes = NVBOOT_AES_BLOCK_LENGTH_BYTES;
    NvU8 uid[NVBOOT_AES_BLOCK_LENGTH_BYTES];
    NvU8 dk[NVBOOT_AES_BLOCK_LENGTH_BYTES];
    NvU64 uid64; // TODO PS -- change fuse api to take an NvU8 pointer
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

    // Get Unique ID from the fuse (64 bit = 8 bytes)
    //Get the size of Uid
    UidSize = sizeof(Nv3pChipUniqueId);
    // Get Uid
    NvDdkFuseGet(NvDdkFuseDataType_Uid, (void *)&uid64, (NvU32 *)&UidSize);
    // TODO PS -- change NvBootFuseGetUniqueId() to return an array of bytes
    //            instead of an NvU64
    //
    // until then, swap byte ordering to make SSK calculation cleaner;
    // after the fix, the Unique ID can be read directly into the uid[] array
    // and the variable uid64 can be eliminated
    for (i = 0; i < 8; i++) {
        uid[7-i] = uid64 & 0xff;
        uid64 >>= 8;
    }

    // copy the 64 bit of UID again at the end of first UID
    // to make 128 bit AES block.
    NvOsMemcpy(&uid[8], &uid[0], BlockLengthBytes >> 1);

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
    NvBootClocksStartPll(NvBootClocksPllId_PllM,
        pData->PllMInputDivider,   // M
        pData->PllMFeedbackDivider,   // N
        pData->PllMPostDivider,   // P
        pData->PllMChargePumpSetupControl,   // CPCON
        pData->PllMLoopFilterSetupControl,   // LPCON
        &StableTime);

    NvOsWaitUS(100000);
}

NvU32 NvMlUtilsGetPLLPFreq(void)
{
    return PLLP_FIXED_FREQ_KHZ_216000;
}

#if NO_BOOTROM

void NvMlUtilsPreInitBit(NvBootInfoTable *pBitTable)
{
    // Nothing to do for T30.
}

NvBool NvMlUtilsStartUsbEnumeration()
{
    //do nothing for ap20 and t30;
    return NV_TRUE;
}

#endif
