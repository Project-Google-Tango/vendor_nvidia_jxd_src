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
 * <b>NVIDIA Driver Development Kit: Operating Modes</b>
 *
 */

#ifndef INCLUDED_NVDDK_OPERATINGMODES_H
#define INCLUDED_NVDDK_OPERATINGMODES_H

/**
 * @defgroup nvddk_operatingmodes_group  Operating Modes
 *
 * Provides operating mode information for NVIDIA Boot Loader functions.
 *
 * @ingroup nvddk_modules
 * @{
 */

/**
 * Defines the supported operating modes.
 */
typedef enum
{
    /// Specifies development mode; boot loader is validated using a fixed key of zeroes.
    NvDdkOperatingMode_NvProduction = 3,

    /// Specifies production mode; boot loader is decrypted using the Secure Boot Key,
    /// then validated using the Secure Boot Key.
    NvDdkOperatingMode_OdmProductionSecure,

    /// Specifies ODM production mode; boot loader is validated using the Secure Boot Key.
    NvDdkOperatingMode_OdmProductionOpen,

    /// Undefined.
    NvDdkOperatingMode_Undefined,

    /// Ignore -- Forces compilers to make 32-bit enums.
    NvDdkOperatingMode_Force32 = 0x7FFFFFFF
} NvDdkOperatingMode;

/** @} */

#endif // INCLUDED_NVDDK_OPERATINGMODES_H

