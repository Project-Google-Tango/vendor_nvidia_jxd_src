/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
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
#include "t11x/arfuse.h"
#include "t11x/arclk_rst.h"
#include "t11x/arapbpm.h"
#include "nvml_aes.h"
#include "nvml_hash.h"
#include "nvboot_usbf_int.h"
#ifdef NVML_NAND_SUPPORT
#include "nvboot_nand_int.h"
#endif
#include "nvboot_usb3_int.h"
#include "nvddk_fuse.h"
#include "t11x/nvboot_se_aes.h"

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
    NvStrapDevSel_Emmc_Primary_x4 = 0,                    // eMMC primary (x4)
    NvStrapDevSel_Emmc_Primary_x8,                        // eMMC primary (x8)
    NvStrapDevSel_Emmc_Secondary_x8,                      // eMMC secondary (x8)
    NvStrapDevSel_Nand,                                   // NAND (x8 or x16)
    NvStrapDevSel_Nand_42nm,                              // NAND (x8)
    NvStrapDevSel_Usb3,                                   // Usb3
    NvStrapDevSel_resvd2,                                 // Usb3
    NvStrapDevSel_Nand_42nm_ef,                           // Nand (x8)
    NvStrapDevSel_SpiFlash,                               // Spi Flash
    NvStrapDevSel_Snor_Muxed_x16,                         // Sync NOR (Muxed, x16)
    NvStrapDevSel_Emmc_Secondary_x4_bootoff,              // eMMC secondary (x4)
    NvStrapDevSel_Emmc_Secondary_x8_bootoff,              // eMMC secondary (x8)
    NvStrapDevSel_Emmc_Secondary_x8_bootoff_30Mhz,        // eMMC secondary (x8)
    NvStrapDevSel_Emmc_Secondary_x8_bootoff_43Mhz,        // eMMC secondary (x8)
    NvStrapDevSel_Emmc_Secondary_x8_bootoff_43Mhz_16page, // eMMC secondary (x8)
    NvStrapDevSel_UseFuses,                               // Use fuses instead

    /* The following definitions must be last. */
    NvStrapDevSel_Num, /* Must appear after the last legal item */
    NvStrapDevSel_Force32 = 0x7FFFFFFF
} NvStrapDevSel;


static NvBootFuseBootDevice StrapDevSelMap[] =
{
    NvBootFuseBootDevice_Sdmmc,       // NvStrapDevSel_Emmc_x4_BootModeOn
    NvBootFuseBootDevice_Sdmmc,       // NvStrapDevSel_Emmc_x8_ootModeOn
    NvBootFuseBootDevice_Sdmmc,       // NvStrapDevSel_Emmc_x8BootModeOff_30Mhz_Multi8_page
    NvBootFuseBootDevice_NandFlash,   // NvStrapDevSel_Nand
    NvBootFuseBootDevice_NandFlash,   // NvStrapDevSel_Nand_42nm
    NvBootFuseBootDevice_Usb3,        // NvStrapDevSel_Usb3
    NvBootFuseBootDevice_Usb3,        // NvStrapDevSel_Usb3
    NvBootFuseBootDevice_NandFlash,   // NvStrapDevSel_Nand_42nm_ef_x8
    NvBootFuseBootDevice_SpiFlash,    // NvStrapDevSel_SpiFlash
    NvBootFuseBootDevice_SnorFlash,   // NvStrapDevSel_Snor_Muxed_x16
    NvBootFuseBootDevice_Sdmmc,       // NvStrapDevSel_Emmc_x4_BootModeOff
    NvBootFuseBootDevice_Sdmmc,       // NvStrapDevSel_Emmc_x8BootModeOff
    NvBootFuseBootDevice_Sdmmc,       // NvStrapDevSel_Emmc_x8BootModeOff_30Mhz
    NvBootFuseBootDevice_Sdmmc,       // NvStrapDevSel_Emmc_x8BootModeOff_43Mh
    NvBootFuseBootDevice_Sdmmc,       // NvStrapDevSel_Emmc_x8BootModeOff_43Mhz_Multi16_page
    NvBootFuseBootDevice_Sdmmc,       // NvStrapDevSel_UseFuses
};


#define NAND_CONFIG(DataWidth, PageOffset, BlockOffset, EnableEF, ToggleDDR) \
    (((DataWidth) << 3) | ((PageOffset) << 6) | ((BlockOffset) << 7) | \
    ((EnableEF) << 11) |((ToggleDDR) << 12))

#define SDMMC_CONFIG(MultiPageSupport, ClockDivider, BusWidth, BootMode) \
    (((BusWidth) << 0) | ((BootMode) << 4) | ((ClockDivider) << 6) | \
    ((MultiPageSupport) << 10))

#define SNOR_CONFIG(NonMuxed, BusWidth) \
    (((BusWidth) << 0) | ((NonMuxed) << 1))

#define SPI_CONFIG(PageSize2kor16k) \
    ((PageSize2kor16k) << 0)

#define XUSB_CONFIG(VBUS_number, OCPin, PortNumber) \
    ((VBUS_number << 8) | (OCPin << 5) | ((PortNumber) << 0))

static NvU32 StrapDevConfigMap[] =
{
    SDMMC_CONFIG(0, 0, 0, 0),   // NvBootStrapDevSel_Emmc_x4_BootModeOn
    SDMMC_CONFIG(0, 0, 1, 0),   // NvBootStrapDevSel_Emmc_x8_BootModeOn
    SDMMC_CONFIG(3, 4, 1, 1),   // NvBootStrapDevSel_Emmc_x8_BootModeOff_ClkDivider_30Mhz_Multi_8_Page
    NAND_CONFIG(0, 0, 0, 0, 1), // NvBootStrapDevSel_Nand
    NAND_CONFIG(1, 1, 1, 0, 0), // NvBootStrapDevSel_Nand_42nm_x8
    XUSB_CONFIG(1, 0, 1),       // NvBootStrapDevSel_Usb3 : Vbus Enable 1, OC Detected 0, port number 1
    XUSB_CONFIG(0, 0, 0),       // NvBootStrapDevSel_Usb3 : Vbus Enable 0, OC Detected 0, port number 0
    NAND_CONFIG(1, 1, 1, 1, 0), // NvBootStrapDevSel_Nand_42nm_EF_x8
    SPI_CONFIG(0),              // NvBootStrapDevSel_SpiFlash : Page size 2K
    SNOR_CONFIG(0, 0),          // NvBootStrapDevSel_Snor_Muxed_x16
    SDMMC_CONFIG(0, 0, 0, 1),   // NvBootStrapDevSel_Emmc_x4_BootModeOff
    SDMMC_CONFIG(0, 0, 1, 1),   // NvBootStrapDevSel_Emmc_x8_BootModeOff
    SDMMC_CONFIG(0, 4, 1, 1),   // NvBootStrapDevSel_Emmc_x8_BootModeOff_ClkDivider_30Mhz
    SDMMC_CONFIG(0, 6, 1, 1),   // NvBootStrapDevSel_Emmc_x8_BootModeOff_clkDivider_43Mhz
    SDMMC_CONFIG(4, 6, 1, 1),   // NvBootStrapDevSel_Emmc_x8_BootModeOff_clkDivider_43Mhz_Multi_16_Page
    0  // No config data        // NvBootStrapDevSel_UseFuses
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
#ifdef NVML_NAND_SUPPORT
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
#else
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
#endif
    {
        /* Callbacks for the SNOR device */
        (NvBootDeviceGetParams)NULL,
        (NvBootDeviceValidateParams)NULL,
        (NvBootDeviceGetBlockSizes)NULL,
        (NvBootDeviceInit)NULL,
        NULL,
        NULL,
        NULL,
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
#ifdef NVML_NAND_SUPPORT
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
#else
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
#endif
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
        /* Callbacks for the Usb3 device */
        (NvBootDeviceGetParams)NvBootUsb3GetParams,
        (NvBootDeviceValidateParams)NvBootUsb3ValidateParams,
        (NvBootDeviceGetBlockSizes)NvBootUsb3GetBlockSizes,
        (NvBootDeviceInit)NvBootUsb3Init,
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
    /* no fusebypass  for t114 at miniloader .keeping the config index
    at safe boot fuse value */
    ConfigIndex = 0x11;
    NvMlUtilsConvertBootFuseDeviceToBootDevice(FuseDev,&BootDevType);

    /* Initialize the device manager. */
    /* This is hanging for boot from usb usecase.
       TODO: it needs to be fixed */
    if (BootDevType != NvBootDevType_Usb3)
        e = NvMlDevMgrInit(&(pContext->DevMgr),
                            BootDevType,
                            ConfigIndex);

    return e;
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
        case Nv3pDeviceType_Emmc:
            *pDevNvBootFuse = NvBootFuseBootDevice_Sdmmc;
            break;
        case Nv3pDeviceType_Usb3:
            *pDevNvBootFuse = NvBootFuseBootDevice_Usb3;
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
        case NvBootFuseBootDevice_Usb3:
            *pNv3pDevice = Nv3pDeviceType_Usb3;
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
        case NvBootFuseBootDevice_SnorFlash:
            *pBootDevice = NvBootDevType_Snor;
            break;
        case NvBootFuseBootDevice_Usb3:
            *pBootDevice = NvBootDevType_Usb3;
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
        case NvBootFuseBootDevice_Usb3:
            DevType = NvBootDevType_Usb3;
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

    NvBootSeAesEncrypt(
        NvBootSeAesKeySlot_SBK,
        1,
        1,
        (NvU8 *)dk,
        (NvU8 *)dk);

    while(NvBootSeIsEngineBusy()) ;

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

    NvBootSeAesEncrypt(
        NvBootSeAesKeySlot_SBK,
        1,
        1,
        (NvU8 *)ssk,
        (NvU8 *)ssk);

    while(NvBootSeIsEngineBusy()) ;

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
    return PLLP_FIXED_FREQ_KHZ_216000;
}

#if NO_BOOTROM

void NvMlUtilsPreInitBit(NvBootInfoTable *pBitTable)
{
    NvOsMemset( (void*) pBitTable, 0, sizeof(NvBootInfoTable) ) ;

    pBitTable->BootRomVersion = NVBOOT_VERSION(0x35,0x01);
    pBitTable->DataVersion = NVBOOT_VERSION(0x35,0x01);
    pBitTable->RcmVersion = NVBOOT_VERSION(0x35,0x01);
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
