/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All Rights Reserved.
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
#include "t12x/nvboot_bct.h"
#include "t12x/arfuse.h"
#include "t12x/nvboot_fuse.h"
#include "nvddk_fuse_common.h"
#include "nvddk_fuse_read_priv_t12x.h"
#include "nvddk_fuse_read.h"

static NvDdkFuseBootDevice StrapDevSelMap[] =
{
    NvDdkFuseBootDevice_Sdmmc,         // NvDdkStrapDevSel_Emmc_x4_BootModeOn
    NvDdkFuseBootDevice_SnorFlash,     // NvDdkStrapDevSel_Snor_Muxed_x16
    NvDdkFuseBootDevice_SnorFlash,     // NvDdkStrapDevSel_Snor_Muxed_x32
    NvDdkFuseBootDevice_SnorFlash,     // NvDdkStrapDevSel_Snor_NonMuxed_x16
    NvDdkFuseBootDevice_Sdmmc,         // NvDdkStrapDevSel_Emmc_x8_BootModeOn
    NvDdkFuseBootDevice_Sdmmc,         // NvDdkStrapDevSel_Emmc_x8_BootModeOff_ClkDivider_31Mhz_Multi_8_Page
    NvDdkFuseBootDevice_Sdmmc,         // NvDdkStrapDevSel_Emmc_x8_BootModeOff_clkDivider_45Mhz_Multi_16_Page
    NvDdkFuseBootDevice_SnorFlash,     // NvDdkStrapDevSel_Snor_NonMuxed_x32
    NvDdkFuseBootDevice_SpiFlash,      // NvDdlStrapDevSel_SpiFlash
    NvDdkFuseBootDevice_resvd_3,       // Reserved; Default to RCM
    NvDdkFuseBootDevice_Sdmmc,         // NvDdkStrapDevSel_Emmc_BMOff_x4
    NvDdkFuseBootDevice_Sdmmc,         // NvDdkStrapDevSel_Emmc_BMOff_x8
    NvDdkFuseBootDevice_Sdmmc,         // NvDdkStrapDevSel_Emmc_BMOffDiv6_x8_BootModeOff
    NvDdkFuseBootDevice_resvd_3,       // Reserved; Default to RCM
    NvDdkFuseBootDevice_Sdmmc,         // NvDdkStrapDevSel_Emmc_BMOff_Div6_x8
    NvDdkFuseBootDevice_Sdmmc,         // NvDdkStrapDdevSel_UseFuses
};

static NvBootDevType MapFuseDevToBootDevType[] =
{
    NvBootDevType_Sdmmc,         // NvBootFuseBootDevice_Sdmmc
    NvBootDevType_Snor,          // NvBootFuseBootDevice_SnorFlash
    NvBootDevType_Spi,           // NvBootFuseBootDevice_SpiFlash
    NvBootDevType_None,          // NvBootFuseBootDevice_None
    NvBootDevType_None,          // NvBootFuseBootDevice_None
};

static NvU32 NvDdkFuseGetStrapDevSelectMapT12x(NvU32 RegData)
{
    return StrapDevSelMap[RegData];
}

static void NvDdkFuseGetUniqueIdT12x(NvU64 *pUniqueId)
{
    NvU32       Uid[4];             // Unique ID
    NvU32       Vendor;             // Vendor
    NvU32       Fab;                // Fab
    NvU32       Wafer;              // Wafer
    NvU32       Lot0;               // Lot 0
    NvU32       Lot1;               // Lot 1
    NvU32       X;                  // X-coordinate
    NvU32       Y;                  // Y-coordinate
    NvU32       Rsvd1;              // Reserved
    NvU32       Reg;                // Scratch register

    /** For t12x:

     * Field        Bits     Data
     * (LSB first)
     * --------     ----     ----------------------------------------
     * Reserved       6
     * Y              9      Wafer Y-coordinate
     * X              9      Wafer X-coordinate
     * WAFER          6      Wafer id
     * LOT_0         32      Lot code 0
     * LOT_1         28      Lot code 1
     * FAB            6      FAB code
     * VENDOR         4      Vendor code
     * --------     ----
     * Total        100
     *
     * Gather up all the bits and pieces.
     *
     * <Vendor:4><Fab:6><Lot0:26><Lot0:6><Lot1:26><Lot1:2><Wafer:6><X:9><Y:9><Reserved:6>
     **/

    // Vendor
    Reg = NV_READ32(NV_ADDRESS_MAP_FUSE_BASE + FUSE_OPT_VENDOR_CODE_0);
    Vendor = NV_DRF_VAL(FUSE, OPT_VENDOR_CODE, OPT_VENDOR_CODE, Reg);

    // Fab
    Reg = NV_READ32(NV_ADDRESS_MAP_FUSE_BASE + FUSE_OPT_FAB_CODE_0);
    Fab = NV_DRF_VAL(FUSE, OPT_FAB_CODE, OPT_FAB_CODE, Reg);

    // Lot 0
    Lot0 = 0;
    Reg = NV_READ32(NV_ADDRESS_MAP_FUSE_BASE + FUSE_OPT_LOT_CODE_0_0);
    Lot0 = NV_DRF_VAL(FUSE, OPT_LOT_CODE_0, OPT_LOT_CODE_0, Reg);

    // Lot 1
    Lot1 = 0;
    Reg = NV_READ32(NV_ADDRESS_MAP_FUSE_BASE + FUSE_OPT_LOT_CODE_1_0);
    Lot1 = NV_DRF_VAL(FUSE, OPT_LOT_CODE_1, OPT_LOT_CODE_1, Reg);

    // Wafer
    Reg = NV_READ32(NV_ADDRESS_MAP_FUSE_BASE + FUSE_OPT_WAFER_ID_0);
    Wafer = NV_DRF_VAL(FUSE, OPT_WAFER_ID, OPT_WAFER_ID, Reg);

    // X-coordinate
    Reg = NV_READ32(NV_ADDRESS_MAP_FUSE_BASE + FUSE_OPT_X_COORDINATE_0);
    X = NV_DRF_VAL(FUSE, OPT_X_COORDINATE, OPT_X_COORDINATE, Reg);

    // Y-coordinate
    Reg = NV_READ32(NV_ADDRESS_MAP_FUSE_BASE +FUSE_OPT_Y_COORDINATE_0);
    Y = NV_DRF_VAL(FUSE, OPT_Y_COORDINATE, OPT_Y_COORDINATE, Reg);

    // Reserved bits
    Reg = NV_READ32(NV_ADDRESS_MAP_FUSE_BASE + FUSE_OPT_OPS_RESERVED_0);
    Rsvd1 = NV_DRF_VAL(FUSE, OPT_OPS_RESERVED, OPT_OPS_RESERVED, Reg);

    Reg = 0;
    Reg |= NV_DRF_NUM(ECID,ECID0,RSVD1,Rsvd1);    // <Reserved:6> = 6 bits
    Reg |= NV_DRF_NUM(ECID,ECID0,Y,Y);            // <Y:9><Reserved:6> = 15 bits
    Reg |= NV_DRF_NUM(ECID,ECID0,X,X);            // <X:9><Y:9><Reserved:6> = 24 bits
    Reg |= NV_DRF_NUM(ECID,ECID0,WAFER,Wafer);    // <Wafer:6><X:9>
                                                  // <Y:9><Reserved:6> = 30 bits
    Reg |= NV_DRF_NUM(ECID,ECID0,LOT1,Lot1);      // <Lot1:2><Wafer:6>
                                                  // <X:9><Y:9><Reserved:6> = 32 bits
    Uid[0] = Reg;

    // discard 2 bits as they are already copied into ECID_0 field
    Lot1 >>= 2;

    Reg = 0;                                      // 32 bits are cleared
    Reg |= NV_DRF_NUM(ECID,ECID1,LOT1,Lot1);      // <Lot1:26> = 26 bits
    Reg |= NV_DRF_NUM(ECID,ECID1,LOT0,Lot0);      // <Lot0:6><Lot1:26> = 32 bits
    Uid[1] = Reg;

    // discard 6 bits as they are already copied into ECID_1 field
    Lot0 >>= 6;

    Reg = 0;                                      // 32 bits are cleared
    Reg |= NV_DRF_NUM(ECID,ECID2,LOT0,Lot0);      // <Lot0:26> = 26 bits
    Reg |= NV_DRF_NUM(ECID,ECID2,FAB,Fab);        // <Fab:6><Lot0:26> = 32 bits
    Uid[2] = Reg;

    Reg = 0;                                      // 32 bits are cleared
    Reg |= NV_DRF_NUM(ECID,ECID3,VENDOR,Vendor);  // <Vendor:4> = 4 bits
    Uid[3] = Reg;
    NvOsMemcpy((void *)pUniqueId, (void *)&Uid, NVDDK_UID_BYTES);
}

static NvU32 NvDdkFuseGetFuseToDdkDevMapT12x(NvU32 RegData)
{
#ifdef BOOT_MINIMAL_BL
    return NvBootDevType_Sdmmc;
#else
    if (RegData >= (int)((sizeof(MapFuseDevToBootDevType)) / sizeof(NvBootDevType)))
        return  -1;
    return MapFuseDevToBootDevType[RegData];
#endif
}

static NvU32 NvDdkFuseGetSecBootDevInstanceT12x(NvU32 BootDev,
                                                NvU32 BootDevConfig)
{
    if (NvBootDevType_Sdmmc == BootDev)
    {
        // only one instance is supported.
        return SDMMC_CONTROLLER_INSTANCE_3;
    }
    else if (NvBootDevType_Usb3 == BootDev)
    {
        /* Boot Config[3:0]
         *  0x00  Port0
         *  0x01  Port1
         *  0x02  port2
         *  0x03  port3
         */
        return BootDevConfig & NVDDK_USB3_PORT_MASK;
    }
    else if (NvBootDevType_Spi == BootDev)
    {
        // Needs Confirmation
        return 3;
    }
    // default
    return 0;
}

static NvStrapDevSel NvStrapDeviceSelectionT12x(void)
{
    NvU32 RegData;
    NvU8 *pMisc;
    NvError e;

    NV_CHECK_ERROR(
        NvOsPhysicalMemMap(PMC_BASE, MISC_PA_LEN,
            NvOsMemAttribute_Uncached, NVOS_MEM_READ_WRITE, (void **)&pMisc));

    RegData = NV_READ32(pMisc + APBDEV_PMC_STRAPPING_OPT_A_0);

    NvOsPhysicalMemUnmap(pMisc, MISC_PA_LEN);

    return (NvStrapDevSel)(NV_DRF_VAL(APBDEV_PMC,
                                          STRAPPING_OPT_A,
                                          BOOT_SELECT,
                                          RegData));
}

static NvU32 NvDdkFuseGetSecBootDeviceRawT12x(void)
{
    NvU32 RegData;
    NvStrapDevSel StrapDevSel;

#ifdef BOOT_MINIMAL_BL
    return NvDdkFuseBootDevice_Sdmmc;
#else
    StrapDevSel = NvStrapDeviceSelectionT12x();
    /* Determine if the fuse data should be overwritten by strap data. */
    if (!NvDdkFuseSkipDevSelStraps() && StrapDevSel != NvStrapDevSel_UseFuses)
    {
        /* Handle out-of-bound values */
        if (StrapDevSel >= NvStrapDevSel_Num)
            StrapDevSel = NvStrapDevSel_Emmc_Primary_x4;
        RegData     = NvDdkFuseGetStrapDevSelectMapT12x(StrapDevSel);
    }
    else
    {
        RegData = FUSE_NV_READ32(FUSE_RESERVED_SW_0);
        RegData = NV_DRF_VAL(FUSE, RESERVED_SW, BOOT_DEVICE_SELECT, RegData);
    }
    return RegData;
#endif
}

static NvU32 NvDdkFuseGetSecBootDeviceT12x(void)
{
    NvU32 RegData;

    RegData = NvDdkFuseGetSecBootDeviceRawT12x();
    return NvDdkFuseGetFuseToDdkDevMapT12x(RegData);
}

static NvBool NvDdkFuseIsPkcBootMode(void)
{
    NvU32 RegValue;

    RegValue = NV_READ32(NV_ADDRESS_MAP_FUSE_BASE + FUSE_PKC_DISABLE_0);
    if(RegValue > 0)
        return NV_FALSE;
    else
        return NV_TRUE;
}

static void NvDdkFuseGetOpModeT12x(NvDdkFuseOperatingMode *pMode)
{
    if (NvDdkFuseIsFailureAnalysisMode())
    {
        *pMode = NvDdkFuseOperatingMode_FailureAnalysis;
    }
    else if (NvDdkFuseIsOdmProductionModeFuseSet())
    {
        if(NvDdkFuseIsPkcBootMode())
        {
            *pMode = NvDdkFuseOperatingMode_OdmProductionSecurePKC;
        }
        else if (NvDdkFuseIsSbkSet())
        {
            *pMode = NvDdkFuseOperatingMode_OdmProductionSecureSBK;
        }
        else
        {
            *pMode = NvDdkFuseOperatingMode_OdmProductionNonSecure;
        }
    }
    else if (NvDdkFuseIsNvProductionModeFuseSet())
    {
        *pMode = NvDdkFuseOperatingMode_NvProduction;
    }
    else
    {
        *pMode = NvDdkFuseOperatingMode_Preproduction;
    }
}
static NvError NvDdkFuseGetPublicKeyHashT12x(NvU8 *pKey)
{

    return NvDdkFuseCopyBytes(FUSE_PUBLIC_KEY0_0,
                              pKey, ARSE_SHA256_HASH_SIZE/8);
}

static NvBool NvDdkFuseIsWatchdogEnabledT12x(void)
{
    NvU32 RegData;

    RegData = NV_READ32(NV_ADDRESS_MAP_FUSE_BASE + FUSE_RESERVED_SW_0);
    RegData = NV_DRF_VAL(FUSE, RESERVED_SW, ENABLE_WATCHDOG, RegData);

    if (RegData)
        return NV_TRUE;
    else
        return NV_FALSE;
}

static NvBool NvDdkFuseIsUsbChargerDetectionEnabledT12x(void)
{
    NvU32 RegData;

    RegData = NV_READ32(NV_ADDRESS_MAP_FUSE_BASE + FUSE_RESERVED_SW_0);
    RegData = NV_DRF_VAL(FUSE, RESERVED_SW, ENABLE_CHARGER_DETECT, RegData);

    if (RegData)
        return NV_TRUE;
    else
        return NV_FALSE;
}

NvError NvDdkFusePrivGetDataT12x(
    NvDdkFuseDataType Type,
    void *pData,
    NvU32 *Size)
{
    NvU32 RegData;
    NvU32 BootDev;
    NvU32 Config;

    if (!Size || !pData)
        return NvError_InvalidAddress;

    switch (Type)
    {
        case NvDdkFuseDataType_SecureBootKey:
            return  NvDdkFuseCopyBytes(FUSE_PRIVATE_KEY0_0,
                                       pData,
                                       NVDDK_SECURE_BOOT_KEY_BYTES);
            break;

        case NvDdkFuseDataType_ReservedOdm:
            return NvDdkFuseCopyBytes(FUSE_RESERVED_ODM0_0,
                                      pData,
                                      NVDDK_RESERVED_ODM_BYTES);
            break;

        case NvDdkFuseDataType_PackageInfo:
            RegData = FUSE_NV_READ32(FUSE_PACKAGE_INFO_0);
            RegData = NV_DRF_VAL(FUSE, PACKAGE_INFO, PACKAGE_INFO, RegData);
            *((NvU32*)pData) = RegData;
            break;

        case NvDdkFuseDataType_SecBootDeviceSelect:
            RegData = NvDdkFuseGetSecBootDeviceT12x();
            if ((NvS32)RegData < 0)
                return NvError_InvalidState;
            *((NvU32*)pData) = RegData;
            break;

        case NvDdkFuseDataType_SecBootDeviceSelectRaw:
            RegData = NvDdkFuseGetSecBootDeviceRawT12x();
            *((NvU32*)pData) = RegData;
            break;

        case NvDdkFuseDataType_Uid:
            SetFuseRegVisibility(1);
            NvDdkFuseGetUniqueIdT12x((NvU64*)pData);
            SetFuseRegVisibility(0);
            break;

        case NvDdkFuseDataType_OpMode:
            NvDdkFuseGetOpModeT12x((NvDdkFuseOperatingMode *)pData);
            break;

        case NvDdkFuseDataType_SecBootDevInst:
            BootDev = NvDdkFuseGetSecBootDeviceT12x();
            if ((NvS32)BootDev < 0)
                return NvError_InvalidState;
            Config = NvDdkFuseGetBootDevInfo();
            RegData = NvDdkFuseGetSecBootDevInstanceT12x(BootDev, Config);
            if ((NvS32)RegData < 0)
                return NvError_InvalidState;
            *((NvU32 *)pData) = RegData;
            break;

        case NvDdkFuseDataType_PublicKeyHash:
            return NvDdkFuseGetPublicKeyHashT12x((NvU8*)pData);
            break;

        case NvDdkFuseDataType_WatchdogEnabled:
            *((NvBool*)pData) = NvDdkFuseIsWatchdogEnabledT12x();
            break;

        case NvDdkFuseDataType_UsbChargerDetectionEnabled:
            *((NvBool*)pData) = NvDdkFuseIsUsbChargerDetectionEnabledT12x();
            break;

        case NvDdkFuseDataType_PkcDisable:
            *((NvBool*)pData) = NvDdkFuseIsPkcBootMode();
            break;

        case NvDdkFuseDataType_Vp8Enable:
            RegData = FUSE_NV_READ32(FUSE_VP8_ENABLE_0);
            *((NvBool*)pData) = RegData ? NV_TRUE : NV_FALSE;
            break;

        case NvDdkFuseDataType_OdmLock:
            RegData = FUSE_NV_READ32(FUSE_ODM_LOCK_0);
            *((NvU32*)pData) = RegData;
            break;

        case NvDdkFuseDataType_CpuSpeedo0:
           *((NvU32 *)pData) = NV_READ32(NV_ADDRESS_MAP_FUSE_BASE +
                                         FUSE_CPU_SPEEDO_0_CALIB_0);
            break;

        case NvDdkFuseDataType_CpuSpeedo1:
           *((NvU32 *)pData) = NV_READ32(NV_ADDRESS_MAP_FUSE_BASE +
                                         FUSE_CPU_SPEEDO_1_CALIB_0);
            break;

        case NvDdkFuseDataType_CpuSpeedo2:
           *((NvU32 *)pData) = NV_READ32(NV_ADDRESS_MAP_FUSE_BASE +
                                         FUSE_CPU_SPEEDO_2_CALIB_0);
            break;

        case NvDdkFuseDataType_CpuIddq:
           *((NvU32 *)pData) = NV_READ32(NV_ADDRESS_MAP_FUSE_BASE +
                                         FUSE_CPU_IDDQ_CALIB_0);
            break;

        case NvDdkFuseDataType_SocSpeedo0:
           *((NvU32 *)pData) = NV_READ32(NV_ADDRESS_MAP_FUSE_BASE +
                                         FUSE_SOC_SPEEDO_0_CALIB_0);
            break;

        case NvDdkFuseDataType_SocSpeedo1:
           *((NvU32 *)pData) = NV_READ32(NV_ADDRESS_MAP_FUSE_BASE +
                                         FUSE_SOC_SPEEDO_1_CALIB_0);
            break;

        case NvDdkFuseDataType_SocSpeedo2:
           *((NvU32 *)pData) = NV_READ32(NV_ADDRESS_MAP_FUSE_BASE +
                                         FUSE_SOC_SPEEDO_2_CALIB_0);
            break;

        case NvDdkFuseDataType_SocIddq:
           *((NvU32 *)pData) = NV_READ32(NV_ADDRESS_MAP_FUSE_BASE +
                                         FUSE_SOC_IDDQ_CALIB_0);
            break;

        default:
            return NvError_NotImplemented;
    }
    return NvSuccess;
}

NvError NvDdkFusePrivGetTypeSizeT12x(NvDdkFuseDataType Type,
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

        case NvDdkFuseDataType_PublicKeyHash:
            *pSize = ARSE_SHA256_HASH_SIZE/8;
            break;

        case NvDdkFuseDataType_CpuSpeedo0:
            *pSize = sizeof(NvU32);
            break;

        case NvDdkFuseDataType_CpuSpeedo1:
            *pSize = sizeof(NvU32);
            break;

        case NvDdkFuseDataType_CpuSpeedo2:
            *pSize = sizeof(NvU32);
            break;

        case NvDdkFuseDataType_CpuIddq:
            *pSize = sizeof(NvU32);
            break;

        case NvDdkFuseDataType_SocSpeedo0:
            *pSize = sizeof(NvU32);
            break;

        case NvDdkFuseDataType_SocSpeedo1:
            *pSize = sizeof(NvU32);
            break;

        case NvDdkFuseDataType_SocSpeedo2:
            *pSize = sizeof(NvU32);
            break;

        case NvDdkFuseDataType_SocIddq:
            *pSize = sizeof(NvU32);
            break;

        default:
             return NvError_NotImplemented;
    }
    return NvSuccess;
}

