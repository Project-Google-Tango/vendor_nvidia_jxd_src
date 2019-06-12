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
#include "t30/nvboot_bct.h"
#include "t30/arfuse.h"
#include "nvddk_fuse_common.h"
#include "nvddk_fuse_read_priv_t30.h"
#include "nvddk_fuse_read.h"

static NvDdkFuseBootDevice StrapDevSelMap[] =
{
    NvDdkFuseBootDevice_Sdmmc,         // NvStrapDevSel_Emmc_Primary_x4
    NvDdkFuseBootDevice_Sdmmc,         // NvStrapDevSel_Emmc_Primary_x8
    NvDdkFuseBootDevice_Sdmmc,         // NvStrapDevSel_Emmc_Secondary_x4
    NvDdkFuseBootDevice_NandFlash,     // NvStrapDevSel_Nand
    NvDdkFuseBootDevice_NandFlash,     // NvStrapDevSel_Nand_42nm
    NvDdkFuseBootDevice_MobileLbaNand, // NvStrapDevSel_MobileLbaNand
    NvDdkFuseBootDevice_MuxOneNand,    // NvStrapDevSel_MuxOneNand
    NvDdkFuseBootDevice_NandFlash,     // NvStrapDevSel_Nand
    NvDdkFuseBootDevice_SpiFlash,      // NvStrapDevSel_SpiFlash
    NvDdkFuseBootDevice_SnorFlash,     // NvStrapDevSel_Snor_Muxed_x16
    NvDdkFuseBootDevice_SnorFlash,     // NvStrapDevSel_Snor_Muxed_x32
    NvDdkFuseBootDevice_SnorFlash,     // NvStrapDevSel_Snor_NonMuxed_x16
    NvDdkFuseBootDevice_MuxOneNand,    // NvStrapDevSel_FlexMuxOneNand
    NvDdkFuseBootDevice_Sata,          // NvStrapDevSel_Sata
    NvDdkFuseBootDevice_Sdmmc,         // NvStrapDevSel_Reserved_0xe
    NvDdkFuseBootDevice_Sdmmc,         // NvStrapDevSel_UseFuses
};

static NvBootDevType MapFuseDevToBootDevType[] =
{
    NvBootDevType_Sdmmc,         // NvBootFuseBootDevice_Sdmmc
    NvBootDevType_Snor,          // NvBootFuseBootDevice_SnorFlash
    NvBootDevType_Spi,           // NvBootFuseBootDevice_SpiFlash
    NvBootDevType_Nand,          // NvBootFuseBootDevice_NandFlash
    NvBootDevType_MobileLbaNand, // NvBootFuseBootDevice_MobileLbaNand
    NvBootDevType_MuxOneNand,    // NvBootFuseBootDevice_MuxOneNand
    NvBootDevType_Sata,          // NvBootFuseBootDevice_Sata
    NvBootDevType_Sdmmc3,        // NvBootFuseBootDevice_BootRom_Reserved_Sdmmc3
};

static NvU32 NvDdkFuseGetStrapDevSelectMapT30(NvU32 RegData)
{
    return StrapDevSelMap[RegData];
}

static void NvDdkFuseGetUniqueIdT30(NvU64 *pUniqueId)
{
    NvU64   Uid;            // Unique ID
    NvU32   Vendor;         // Vendor
    NvU32   Fab;            // Fab
    NvU32   Lot;            // Lot
    NvU32   Wafer;          // Wafer
    NvU32   X;              // X-coordinate
    NvU32   Y;              // Y-coordinate
    NvU32   Cid;            // Chip id
    NvU32   Reg;            // Scratch register
    NvU32   i;

    Cid = 0;
    // Vendor
    Reg = NV_READ32(NV_ADDRESS_MAP_FUSE_BASE + FUSE_OPT_VENDOR_CODE_0);
    Vendor = NV_DRF_VAL(FUSE, OPT_VENDOR_CODE, OPT_VENDOR_CODE, Reg);

    // Fab
    Reg = NV_READ32(NV_ADDRESS_MAP_FUSE_BASE + FUSE_OPT_FAB_CODE_0);
    Fab = NV_DRF_VAL(FUSE, OPT_FAB_CODE, OPT_FAB_CODE, Reg);

    // Lot code must be re-encoded from a 5 digit base-36 'BCD' number
    // to a binary number.
    Lot = 0;
    Reg = NV_READ32(NV_ADDRESS_MAP_FUSE_BASE + FUSE_OPT_LOT_CODE_0_0);
    Reg <<= 2;  // Two unused bits (5 6-bit 'digits' == 30 bits)
    for (i = 0; i < 5; ++i)
    {
        NvU32 Digit;

        Digit = (Reg & 0xFC000000) >> 26;
        NV_ASSERT(Digit < 36);
        Lot *= 36;
        Lot += Digit;
        Reg <<= 6;
    }

    // Wafer
    Reg = NV_READ32(NV_ADDRESS_MAP_FUSE_BASE + FUSE_OPT_WAFER_ID_0);
    Wafer = NV_DRF_VAL(FUSE, OPT_WAFER_ID, OPT_WAFER_ID, Reg);

    // X-coordinate
    Reg = NV_READ32(NV_ADDRESS_MAP_FUSE_BASE + FUSE_OPT_X_COORDINATE_0);
    X = NV_DRF_VAL(FUSE, OPT_X_COORDINATE, OPT_X_COORDINATE, Reg);

    // Y-coordinate
    Reg = NV_READ32(NV_ADDRESS_MAP_FUSE_BASE + FUSE_OPT_Y_COORDINATE_0);
    Y = NV_DRF_VAL(FUSE, OPT_Y_COORDINATE, OPT_Y_COORDINATE, Reg);

    Uid = NV_DRF64_NUM(UID, UID, CID, Cid)
        | NV_DRF64_NUM(UID, UID, VENDOR, Vendor)
        | NV_DRF64_NUM(UID, UID, FAB, Fab)
        | NV_DRF64_NUM(UID, UID, LOT, Lot)
        | NV_DRF64_NUM(UID, UID, WAFER, Wafer)
        | NV_DRF64_NUM(UID, UID, X, X)
        | NV_DRF64_NUM(UID, UID, Y, Y);

    *pUniqueId = Uid;
}

static NvU32 NvDdkFuseGetFuseToDdkDevMapT30(NvU32 RegData)
{
    if (RegData >= (int)((sizeof(MapFuseDevToBootDevType)) / sizeof(NvBootDevType)))
        return  -1;
    return MapFuseDevToBootDevType[RegData];
}

static NvU32 NvDdkFuseGetSecBootDevInstanceT30(NvU32 BootDev,
                                               NvU32 BootDevConfig)
{
    if (NvBootDevType_Sdmmc == BootDev)
    {
        // Please refer to T30 bootrom wiki for the below logic
        if (BootDevConfig & (1 << BOOT_DEVICE_CONFIG_INSTANCE_SHIFT))
            return SDMMC_CONTROLLER_INSTANCE_2;
        else
            return SDMMC_CONTROLLER_INSTANCE_3;
    }
    else if (NvBootDevType_Spi == BootDev)
    {
        return 3;
    }
    // defualt
    return 0;
}
static NvStrapDevSel NvStrapDeviceSelectionT30(void)
{
    NvU32 RegData;
    NvU8 *pMisc;
    NvError e;

    NV_CHECK_ERROR(
        NvOsPhysicalMemMap(MISC_PA_BASE, MISC_PA_LEN,
            NvOsMemAttribute_Uncached, NVOS_MEM_READ_WRITE, (void **)&pMisc));

    RegData = NV_READ32(pMisc + APB_MISC_PP_STRAPPING_OPT_A_0);

    NvOsPhysicalMemUnmap(pMisc, MISC_PA_LEN);

    return (NvStrapDevSel)(NV_DRF_VAL(APB_MISC_PP,
                                          STRAPPING_OPT_A,
                                          BOOT_SELECT,
                                          RegData));
}

static NvU32 NvDdkFuseGetSecBootDeviceRawT30(void)
{
    NvU32 RegData;
    NvStrapDevSel StrapDevSel;
    StrapDevSel = NvStrapDeviceSelectionT30();

    /* Determine if the fuse data should be overwritten by strap data. */
    if (!NvDdkFuseSkipDevSelStraps() && StrapDevSel != NvStrapDevSel_UseFuses)
    {
        /* Handle out-of-bound values */
        if (StrapDevSel >= NvStrapDevSel_Num)
            StrapDevSel = NvStrapDevSel_Emmc_Primary_x4;

        RegData     = NvDdkFuseGetStrapDevSelectMapT30(StrapDevSel);
    }
    else
    {
        RegData = FUSE_NV_READ32(FUSE_RESERVED_SW_0);
        RegData = NV_DRF_VAL(FUSE, RESERVED_SW, BOOT_DEVICE_SELECT, RegData);
    }
    return RegData;
}

static NvU32 NvDdkFuseGetSecBootDeviceT30(void)
{
    NvU32 RegData;

    RegData = NvDdkFuseGetSecBootDeviceRawT30();
    return NvDdkFuseGetFuseToDdkDevMapT30(RegData);
}

static void NvDdkFuseGetOpModeT30(NvDdkFuseOperatingMode *pMode)
{
    if (NvDdkFuseIsFailureAnalysisMode())
    {
        *pMode = NvDdkFuseOperatingMode_FailureAnalysis;
    }
    else
    {
        if (NvDdkFuseIsOdmProductionModeFuseSet())
        {
            if (NvDdkFuseIsSbkSet())
            {
                 *pMode = NvDdkFuseOperatingMode_OdmProductionSecure;
            }
            else
            {
                *pMode = NvDdkFuseOperatingMode_OdmProductionOpen;
            }
        }
        else
        {
            if (NvDdkFuseIsNvProductionModeFuseSet())
            {
                *pMode = NvDdkFuseOperatingMode_NvProduction;
            }
            else
            {
                *pMode = NvDdkFuseOperatingMode_Preproduction;
            }
        }
    }
}

static void NvDdkFuseReadSpareBits(NvDdkFuseDataType Type, void *pData)
{
    NvU32 LinearAge = 0, AgeBit  = 0;

    switch(Type)
    {
        case NvDdkFuseDataType_GetAge:
            FUSE_READ_AGE_BIT(6, AgeBit, LinearAge);
            FUSE_READ_AGE_BIT(5, AgeBit, LinearAge);
            FUSE_READ_AGE_BIT(4, AgeBit, LinearAge);
            FUSE_READ_AGE_BIT(3, AgeBit, LinearAge);
            FUSE_READ_AGE_BIT(2, AgeBit, LinearAge);
            FUSE_READ_AGE_BIT(1, AgeBit, LinearAge);
            FUSE_READ_AGE_BIT(0, AgeBit, LinearAge);
            /*Default Aug, 2012*/
            if (LinearAge <= 0)
                LinearAge = 8;
            *((NvU32 *)pData) = LinearAge;
            break;
        default:
            return;
    }
    return;
}


NvError NvDdkFusePrivGetDataT30(
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
            return NvDdkFuseCopyBytes(FUSE_PRIVATE_KEY0_0,
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
            RegData = NvDdkFuseGetSecBootDeviceT30();
            if ((NvS32)RegData < 0)
                return NvError_InvalidState;
            *((NvU32*)pData) = RegData;
            break;

        case NvDdkFuseDataType_SecBootDeviceSelectRaw:
            RegData = NvDdkFuseGetSecBootDeviceRawT30();
            *((NvU32*)pData) = RegData;
            break;

        case NvDdkFuseDataType_Uid:
            SetFuseRegVisibility(1);
            NvDdkFuseGetUniqueIdT30((NvU64*)pData);
            SetFuseRegVisibility(0);
            break;

        case NvDdkFuseDataType_OpMode:
            NvDdkFuseGetOpModeT30((NvDdkFuseOperatingMode *)pData);
            break;

        case NvDdkFuseDataType_SecBootDevInst:
            BootDev = NvDdkFuseGetSecBootDeviceT30();
            if ((NvS32)BootDev < 0)
                return NvError_InvalidState;
            Config = NvDdkFuseGetBootDevInfo();
            RegData = NvDdkFuseGetSecBootDevInstanceT30(BootDev, Config);
            if ((NvS32)RegData < 0)
                return NvError_InvalidState;
            *((NvU32 *)pData) = RegData;
            break;

        case NvDdkFuseDataType_GetAge:
            NvDdkFuseReadSpareBits(Type,pData);
            break;

        default:
            return NvError_NotImplemented;
    }
    return NvSuccess;
}

NvError NvDdkFusePrivGetTypeSizeT30(NvDdkFuseDataType Type,
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
