/*
 * Copyright (c) 2012-2013, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_NVDDK_T30_FUSE_PRIV_H
#define INCLUDED_NVDDK_T30_FUSE_PRIV_H

#if defined(__cplusplus)
extern "C"
{
#endif

/*
 * NvStrapDevSel: Enumerated list of devices selectable via straps.
 */
typedef enum
{
    NvStrapDevSel_Emmc_Primary_x4 = 0, /* eMMC primary (x4)          */
    NvStrapDevSel_Emmc_Primary_x8,     /* eMMC primary (x8)          */
    NvStrapDevSel_Emmc_Secondary_x4,   /* eMMC secondary (x4)        */
    NvStrapDevSel_Nand,                /* NAND (x8 or x16)           */
    NvStrapDevSel_Nand_42nm,           /* NAND (x8 or x16)           */
    NvStrapDevSel_MobileLbaNand,       /* mobileLBA NAND             */
    NvStrapDevSel_MuxOneNand,          /* MuxOneNAND                 */
    NvStrapDevSel_Nand_42nm_meo_2,     /* NAND (x8 or x16)           */
    NvStrapDevSel_SpiFlash,            /* SPI Flash                  */
    NvStrapDevSel_Snor_Muxed_x16,      /* Sync NOR (Muxed, x16)      */
    NvStrapDevSel_Snor_Muxed_x32,      /* Sync NOR (Muxed, x32)      */
    NvStrapDevSel_Snor_NonMuxed_x16,   /* Sync NOR (NonMuxed, x16)   */
    NvStrapDevSel_FlexMuxOneNand,      /* FlexMuxOneNAND             */
    NvStrapDevSel_Sata,                /* Sata                       */
    NvStrapDevSel_Emmc_Secondary_x8,   /* eMMC secondary (x4)        */
    NvStrapDevSel_UseFuses,            /* Use fuses instead          */

    /* The following definitions must be last. */
    NvStrapDevSel_Num, /* Must appear after the last legal item */
    NvStrapDevSel_Force32 = 0x7FFFFFFF
} NvStrapDevSel;

typedef enum
{
    NvDdkFuseBootDevice_Sdmmc,
    NvDdkFuseBootDevice_SnorFlash,
    NvDdkFuseBootDevice_SpiFlash,
    NvDdkFuseBootDevice_NandFlash,
    NvDdkFuseBootDevice_NandFlash_x8  = NvDdkFuseBootDevice_NandFlash,
    NvDdkFuseBootDevice_NandFlash_x16 = NvDdkFuseBootDevice_NandFlash,
    NvDdkFuseBootDevice_MobileLbaNand,
    NvDdkFuseBootDevice_MuxOneNand,
    NvDdkFuseBootDevice_Sata,
    NvDdkFuseBootDevice_BootRom_Reserved_Sdmmc3,   /* !!! this enum is strictly used by BootRom code only !!! */
    NvDdkFuseBootDevice_Max, /* Must appear after the last legal item */
    NvDdkFuseBootDevice_Force32 = 0x7fffffff
} NvDdkFuseBootDevice;

#define NVDDK_SECURE_BOOT_KEY_BYTES  (16)
#define NVDDK_RESERVED_ODM_BYTES  (32)
#define NVDDK_UID_BYTES (8)

// Local DRF defines for UID fields.
#define UID_UID_0_CID_RANGE     63:60
#define UID_UID_0_VENDOR_RANGE  59:56
#define UID_UID_0_FAB_RANGE     55:50
#define UID_UID_0_LOT_RANGE     49:24
#define UID_UID_0_WAFER_RANGE   23:18
#define UID_UID_0_X_RANGE       17:9
#define UID_UID_0_Y_RANGE       8:0

#define BOOT_DEVICE_CONFIG_INSTANCE_SHIFT   6

#ifdef __cplusplus
}
#endif

#endif
