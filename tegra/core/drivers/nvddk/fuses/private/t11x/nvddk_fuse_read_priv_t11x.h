/*
 * Copyright (c) 2012-2013, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_NVDDK_T11X_FUSE_PRIV_H
#define INCLUDED_NVDDK_T11X_FUSE_PRIV_H

#if defined(__cplusplus)
extern "C"
{
#endif

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
    NvStrapDevSel_Emmc_Secondary_x8_bootoff_43Mhz_16page, //eMMC secondary (x8)
    NvStrapDevSel_UseFuses,                               // Use fuses instead

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
    NvDdkFuseBootDevice_Usb3,
    NvDdkFuseBootDevice_resvd_5,
    NvDdkFuseBootDevice_resvd_6, // for SATA on T40
    NvDdkFuseBootDevice_resvd_7,
    NvDdkFuseBootDevice_Max, /* Must appear after the last legal item */
    NvDdkFuseBootDevice_Force32 = 0x7fffffff
} NvDdkFuseBootDevice;

#define NVDDK_SECURE_BOOT_KEY_BYTES  (16)
#define NVDDK_RESERVED_ODM_BYTES  (32)
#define NVDDK_UID_BYTES (16)

#define NVDDK_USB3_PORT_MASK 0x3

#define FUSE_RESERVED_SW_0_ENABLE_CHARGER_DETECT_RANGE    4:4
#define FUSE_RESERVED_SW_0_ENABLE_WATCHDOG_RANGE          5:5

#define ARSE_SHA256_HASH_SIZE   256

#define NVDDK_TID_BYTES (3)

#ifdef __cplusplus
}
#endif

#endif
