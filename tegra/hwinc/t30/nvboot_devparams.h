/*
 * Copyright (c) 2007 - 2009 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * Definition of the device manager parameters structure.
 *
 * The device parameter structure is a union of the parameter structures
 * of all the classes of secondary boot devices.
 *
 * The values in the BCT's device parameter structures are used to 
 * reinitialize the driver prior to reading BLs.  This mechanism gives the
 * system designer an opportunity to store values tuned for maximum performance
 * with the specific devices in the system.  In contrast, the values initially
 * used by the Boot ROM to read BCTs are geared toward universality across
 * a wide set of potential parts, not tuned to the characteristics of a
 * specific device.
 *
 * The array of device parameter structures in the BCT permits the storage
 * of sets of optimized parameters for different devices, which supports
 * different board stuffing options, multiple sourcing of parts, etc.
 */

#ifndef INCLUDED_NVBOOT_DEVPARAMS_H
#define INCLUDED_NVBOOT_DEVPARAMS_H

#include "nvcommon.h"
#include "t30/nvboot_error.h"

/*
 * Include the declaration of parameter structures for all supported devices.
 */
#include "t30/nvboot_mobile_lba_nand_param.h"
#include "t30/nvboot_mux_one_nand_param.h"
#include "t30/nvboot_nand_param.h"
#include "t30/nvboot_snor_param.h"
#include "t30/nvboot_sdmmc_param.h"
#include "t30/nvboot_spi_flash_param.h"
#include "t30/nvboot_sata_param.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * Defines the union of the parameters required by each device.
 */
typedef union
{
    /// Dummy member to fix the size of NvBootDevParams
    NvU8 Size[64];
    /// Specifies optimized parameters for mobileLA NAND
    NvBootMobileLbaNandParams MobileLbaNandParams;

    /// Specifies optimized parameters for MuxOneNAND and FlexMuxOneNAND
    NvBootMuxOneNandParams    MuxOneNandParams;

    /// Specifies optimized parameters for NAND
    NvBootNandParams          NandParams;

    /// Specifies optimized parameters for eMMC and eSD
    NvBootSdmmcParams         SdmmcParams;

    /// Specifies optimized parameters for SNOR
    NvBootSnorParams          SnorParams;

    /// Specifies optimized parameters for SPI NOR
    NvBootSpiFlashParams      SpiFlashParams;

    /// Specifies optimized parameters for SATA
    NvBootSataParams          SataParams;
} NvBootDevParams;

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_NVBOOT_DEVPARAMS_H */
