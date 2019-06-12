/**
 * @file
 * @brief
 * @b Description:
 *
 */

/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto. Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NAND_CONFIGURATION_H
#define INCLUDED_NAND_CONFIGURATION_H

#include "nvddk_nand.h"
#include "nanddefinitions.h"
#include "nvodm_query_nand.h"

// Maximum NAND flash chip selects supported on board are 8.
// But, some of the pins are shared with DTV and XIO so define the
// MAX_NAND_SUPPORTED to 4 if we are using the DTV on  JIVE board.

enum {MAX_ALLOWED_INTERLEAVE_BANKS = 4};

typedef enum ECCAlgorithm
{
    // Reed solomon algorithm
    ECCAlgorithm_REED_SOLOMON = 0,
    // Hamming algorithm
    ECCAlgorithm_HAMMING,
    ECCAlgorithm_Force32 = 0x7FFFFFFF
}ECCAlgorithm;

// File system to use for formatting Nand flash.
typedef enum NandFSFormatType
{
    // Fat 16 File system.
    NandFSFormatType_NAND_FS_FAT_16 = 0,
    // Fat 32 File system.
    NandFSFormatType_NAND_FS_FAT_32,
    NandFSFormatType_Force32 = 0x7FFFFFFF
}NandFSFormatType;

// This is enummeration status for the structure Information
typedef enum Information_Status
{
    // 0:DataReserved; 1:!DataReserved
    Information_Status_DATA_RESERVED = 0,
    // 0:SystemReserved; 1:!SystemReserved
    Information_Status_SYSTEM_RESERVED = 0,
    // Page status 1:good/0:bad
    Information_Status_PAGE_GOOD = 1,
    Information_Status_PAGE_BAD = 0,
    // 0:ttReserved; 1:!ttReserved
    Information_Status_TT_RESERVED = 0,
    // 0:TatReserved; 1:!TatReserved
    Information_Status_TAT_RESERVED = 0,
    // 0:BbReserved; 1:!BbReserved
    Information_Status_BB_RESERVED = 0,
    // 0:FwReserved; 1:!FwReserved
    Information_Status_FW_RESERVED = 0,
    // 0:NvpReserved; 1:!NvpReserved
    Information_Status_NVP_RESERVED = 0,
    Information_Status_Force32 = 0x7FFFFFFF
}Information_Status;

// WinCE
#ifndef NVBL_FLASH
typedef enum GeneralConfiguration1
{
    // Adding wince and wm600 reserved blocks
    // Blocks reserved for storing primary boot loader
    // Boot rom reserved
    GeneralConfiguration_BLOCKS_FOR_PRIMARY_BOOTLOADER = 128,
    // Eboot = 4 block * 2 copies
    // Blocks reserved for storing secondary boot loader
    GeneralConfiguration_BLOCKS_FOR_SECONDARY_BOOTLOADER = 8,
    // Blocks reserved for storing firmware image
    // Kernel Image or Disk Image for WM600
    GeneralConfiguration_BLOCKS_FOR_CPU_IMAGE_STORAGE = 512,
    // Blocks reserved for storing COP firmware image
    GeneralConfiguration_BLOCKS_FOR_COP_IMAGE_STORAGE = 0,
    // Blocks reserved for storing bitmaps
    GeneralConfiguration_BLOCKS_FOR_BITMAP_STORAGE = 4,
    // Blocks reserved for storing non volatile parameters
    // Boot args 1 block * 2 copies
    GeneralConfiguration_BLOCKS_FOR_NVP_STORAGE = 2,
    // Blocks reserved for storing translation table
    GeneralConfiguration_BLOCKS_FOR_TT_STORAGE = 16,
    // Blocks reserved for storing translation allocation table
    GeneralConfiguration_BLOCKS_FOR_TAT_STORAGE = 8,
    // Blocks reserved for storing bad block information
    GeneralConfiguration_BLOCKS_FOR_BADBLOCK_STORAGE = 2,
    // Specify the ECC algorithm to use for spare area. Refer to the above enumeration
    // "ECCAlgorithm" for the supported algorithms.
    GeneralConfiguration_ECC_ALGORITHM_SPARE_AREA = ECCAlgorithm_HAMMING,
    // Set the threshold of ECC to replace bad block with new
    GeneralConfiguration_ECC_THRESHOLD_SPARE_AREA = 1,

    // Specify the File system format to use. Refer to the above enumeration
    // "NandFSFormatType" for the supported algorithms.
    GeneralConfiguration_NAND_FILE_SYSTEM = NandFSFormatType_NAND_FS_FAT_16,

    // turn off this flag during the development phase to erase the entire Nand flash
    // must be enabled for production release to track the bad blocks properly
    GeneralConfiguration_NAND_PRODUCTION_RELEASE = 0,
    GeneralConfiguration_Force32 = 0x7FFFFFFF
}GeneralConfiguration1;
#else

// WM6
typedef enum GeneralConfiguration1
{
    // Blocks reserved for storing translation table
    GeneralConfiguration_BLOCKS_FOR_TT_STORAGE = 16,
    // Blocks reserved for storing translation allocation table
    GeneralConfiguration_BLOCKS_FOR_TAT_STORAGE = 8,
    // Blocks reserved for storing bad block information
    GeneralConfiguration_BLOCKS_FOR_BADBLOCK_STORAGE = 2,
    // Specify the ECC algorithm to use for spare area. Refer to the above enumeration
    // "ECCAlgorithm" for the supported algorithms.
    GeneralConfiguration_ECC_ALGORITHM_SPARE_AREA = ECCAlgorithm_HAMMING,
    // Set the threshold of ECC to replace bad block with new
    GeneralConfiguration_ECC_THRESHOLD_SPARE_AREA = 1,

    // Specify the File system format to use. Refer to the above enumeration
    // "NandFSFormatType" for the supported algorithms.
    GeneralConfiguration_NAND_FILE_SYSTEM = NandFSFormatType_NAND_FS_FAT_16,

    // turn off this flag during the development phase to erase the entire Nand flash
    // must be enabled for production release to track the bad blocks properly
    GeneralConfiguration_NAND_PRODUCTION_RELEASE = 0,
    GeneralConfiguration_Force32 = 0x7FFFFFFF
}GeneralConfiguration1;
#endif
typedef struct BlockConfigurationRec
{
    NvU32    NoOfTatBlks;
    NvU32    NoOfTtBlks;
    NvU32    NoOfFbbMngtBlks;

    NvU32    PhysBlksPerZone;
    NvU32    LogBlksPerZone;
    // Virtual Banks Per Device
    NvU32    VirtBanksPerDev;
    NvU32    NoOfBanks;
    NvU32    PhysBlksPerBank;
    // Interleave Bank count
    NvU32    ILBankCount;
    NvOdmNandInterleaveCapability Capability;
    ECCAlgorithm    DataAreaEccAlgorithm;
    NvU32    DataAreaEccThreshold;
}BlockConfiguration;

    NvU32 NandConfigGetNumberOfttBlocks(NvNandHandle hNand);
    NvU32 NandConfigGetNumberOfReservedBlocks(NvNandHandle hNand);
    NvU32 NandConfigGetNumberOfFixedSystemBlocks(NvNandHandle hNand);

    NvS32 NandConfigGetTotalNumberOfBanks(NvNandHandle hNand);
#endif /* INCLUDED_NAND_CONFIGURATION_H */
