/*
 * Copyright (c) 2010-2012, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */


/**
 * @file
 * <b>NVIDIA Driver Development Kit: Boot Loader Device Types</b>
 *
 */

#ifndef INCLUDED_NVDDK_BOOTDEVICES_H
#define INCLUDED_NVDDK_BOOTDEVICES_H

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @defgroup nvddk_bootloader_group Boot Device Types
 *
 * Provides types of boot devices for NVIDIA Tegra functions.
 *
 * @ingroup nvddk_modules
 * @{
 */

/**
 * Defines the supported secondary boot device types.
 */
typedef enum
{
    /// Specifies NAND flash (SLC and MLC).
    NvDdkSecBootDeviceType_Nand = 1,

    /// Specifies NOR flash.
    NvDdkSecBootDeviceType_Nor,

    /// Specifies SPI flash.
    NvDdkSecBootDeviceType_Spi,

    /// Specifies eMMC flash
    /// @note eSD is only supported on Tegra 2 devices.
    NvDdkSecBootDeviceType_eMMC,

    /// Specifies 16-bit NAND flash.
    NvDdkSecBootDeviceType_Nand_x16,

    /// Specifies mobileLBA NAND flash (Tegra 2 only).
    NvDdkSecBootDeviceType_MobileLbaNand,

    /// Specifies SD flash.
    /// @note eSD is only supported on Tegra 2 devices.
    NvDdkSecBootDeviceType_Sdmmc,

    /// Specifies MuxOneNAND flash (Tegra 2 & 3 only).
    NvDdkSecBootDeviceType_MuxOneNand,

    /// Specifies Sata (Tegra 3 only).
    NvDdkSecBootDeviceType_Sata,

    /// Specifies Sdmmc3 (Tegra 3 only).
    NvDdkSecBootDeviceType_Sdmmc3,


    /// Specifies the maximum number of flash device types
    /// -- Should appear after the last legal item.
    NvDdkSecBootDeviceType_Max,

    /// Undefined.
    NvDdkSecBootDeviceType_Undefined,

    /// Ignore -- Forces compilers to make 32-bit enums.
    NvDdkSecBootDeviceType_Force32 = 0x7FFFFFFF
} NvDdkSecBootDeviceType;

/** @} */

#ifdef __cplusplus
}
#endif

#endif // INCLUDED_NVDDK_BOOTDEVICES_H

