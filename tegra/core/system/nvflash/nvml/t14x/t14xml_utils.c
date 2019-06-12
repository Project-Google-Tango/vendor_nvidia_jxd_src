/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvml_utils.h"
#include "nvml_device.h"
#include "nvddk_fuse.h"
#include "t14x/nvboot_se_aes.h"

#define MISC_PA_BASE 0x70000000UL
#define MISC_PA_LEN  4096
#define PMC_BASE    0x7000E400
#define CLK_RST_PA_BASE   0x60006000
#define PLLM_ENABLE_SHIFT 12

#define FUSE_RESERVED_SW_0_SKIP_DEV_SEL_STRAPS_RANGE  3:3

extern NvBctHandle BctDeviceHandle;

/*
 * NvStrapDevSel: Enumerated list of devices selectable via straps.
 */
typedef enum
{
    NvStrapDevSel_UseFuses = 0,    /* Use fuses instead  */
    NvStrapDevSel_SpiFlash,        /* SPI Flash  */
    NvStrapDevSel_Emmc_Boot_on,    /* eMMC (x8) Boot mode on */
    NvStrapDevSel_Emmc_Boot_off,   /* eMMC (x8) Boot mode off */

    /* The following definitions must be last. */
    NvStrapDevSel_Num, /* Must appear after the last legal item */
    NvStrapDevSel_Force32 = 0x7FFFFFFF
} NvStrapDevSel;



#define SDMMC_CONFIG(MultiPageSupport, ClockDivider, BusWidth, BootMode) \
    (((BusWidth) << 0) | ((BootMode) << 4) | ((ClockDivider) << 6) | ((MultiPageSupport) << 10))

static NvU32 StrapDevConfigMap[] =
{
    0,  //No config data        // NvBootStrapDevSel_UseFuses
    0,  //No config data        // NvBootStrapDevSel_SpiFlash
    SDMMC_CONFIG(0, 5, 1, 1),   // NvBootStrapDevSel_Emmc_x8_Boot_On_512_26MHz
    SDMMC_CONFIG(0, 5, 1, 0),   // NvBootStrapDevSel_Emmc_x8_Boot_Off_512_26MHz
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
        (NvBootDeviceGetParams)NULL,
        (NvBootDeviceValidateParams)NULL,
        (NvBootDeviceGetBlockSizes)NULL,
        (NvBootDeviceInit)NULL,
        NULL,
        NULL,
        NULL,
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
    #if 0 //FIX ME!!
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
    #endif
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
    /* no fusebypass  for t148 at miniloader .keeping the config index
    at safe boot fuse value */
    ConfigIndex = 0x11;
    NvMlUtilsConvertBootFuseDeviceToBootDevice(FuseDev,&BootDevType);

    /* Initialize the device manager. */
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
    return NvStrapDevSel_Emmc_Boot_on;
}

void NvMlUtilsGetBootDeviceType(NvBootFuseBootDevice *pDevType)
{
    NvU32 Size;

    // Read from fuse
    Size = sizeof(NvU32);
    NvDdkFuseGet(NvDdkFuseDataType_SecBootDeviceSelectRaw,
                    (void *)pDevType, (NvU32 *)&Size);

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
            StrapDevSel = NvStrapDevSel_Emmc_Boot_on;

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
                         pData->PllMPostDivider,             // P
                         Misc1,
                         Misc2,
                         &StableTime);
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

    pBitTable->BootRomVersion = NVBOOT_VERSION(0x14,0x01);
    pBitTable->DataVersion = NVBOOT_VERSION(0x14,0x01);
    pBitTable->RcmVersion = NVBOOT_VERSION(0x14,0x01);
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
