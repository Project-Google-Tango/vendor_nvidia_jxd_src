//
// Copyright (c) 2007-2013 NVIDIA Corporation.  All Rights Reserved.
//
// NVIDIA Corporation and its licensors retain all intellectual property and
// proprietary rights in and to this software and related documentation.  Any
// use, reproduction, disclosure or distribution of this software and related
// documentation without an express license agreement from NVIDIA Corporation
// is strictly prohibited.
//

/**
 * @file
 * <b>NVIDIA Tegra Boot Loader API</b>
 *
 */

#ifndef INCLUDED_NVBL_BOOTDEVICES_H
#define INCLUDED_NVBL_BOOTDEVICES_H

/**
 * @defgroup nvbl_bootdevices_group NvBL Boot Device Types
 *
 * Provides types of boot devices for NVIDIA Boot Loader functions.
 *
 * @ingroup nvbl_group
 * @{
 */

/**
 * Defines the supported secondary boot device types.
 */
typedef enum
{
    /// Specifies NAND flash (SLC and MLC).
    NvBlSecBootDeviceType_Nand = 1,

    /// Specifies NOR flash.
    NvBlSecBootDeviceType_Nor,

    /// Specifies SPI flash.
    NvBlSecBootDeviceType_Spi,

    /// Specifies eMMC flash.
    /// @note eSD is only supported on Tegra 2 devices.
    NvBlSecBootDeviceType_eMMC,

    /// Specifies 16-bit NAND flash.
    NvBlSecBootDeviceType_Nand_x16,

    /// Specifies mobileLBA NAND flash (Tegra 2 only).
    NvBlSecBootDeviceType_MobileLbaNand,

    /// Specifies SD flash.
    /// @note eSD is only supported on Tegra 200 series devices.
    NvBlSecBootDeviceType_Sdmmc,

    /// Specifies MuxOneNAND flash (Tegra 2 only).
    NvBlSecBootDeviceType_MuxOneNand,

    /// Specifies SATA(Tegra 3 only).
    NvBlSecBootDeviceType_Sata,

    /// Specifies the maximum number of flash device types
    /// -- Should appear after the last legal item.
    NvBlSecBootDeviceType_Max,

    /// Undefined.
    NvBlSecBootDeviceType_Undefined,

    /// Ignore -- Forces compilers to make 32-bit enums.
    NvBlSecBootDeviceType_Force32 = 0x7FFFFFFF
} NvBlSecBootDeviceType;

/** @} */

#endif // INCLUDED_NVBL_BOOTDEVICES_H

