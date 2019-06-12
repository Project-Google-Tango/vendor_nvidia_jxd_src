/*
 * Copyright (c) 2013, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_NVDDK_T14X_FUSE_PRIV_H
#define INCLUDED_NVDDK_T14X_FUSE_PRIV_H

#if defined(__cplusplus)
extern "C"
{
#endif

/*
 * NvStrapDevSel: Enumerated list of devices selectable via straps.
 */
typedef enum
{
    NvStrapDevSel_UseFuses = 0,    /* Use fuses instead   */
    NvStrapDevSel_SpiFlash,        /* SPI Flash           */
    NvStrapDevSel_Emmc_Primary_x4, /* eMMC primary (x4)   */
    NvStrapDevSel_Emmc_Primary_x8, /* eMMC primary (x8)   */
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

#ifdef __cplusplus
}
#endif

#endif
