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
 * nvboot_config.h provides constants that parameterize operations in the
 * Boot ROM.
 *
 * IMPORTANT: These constants can ONLY be changed during Boot ROM 
 *            development.  For a particular Tegra product, their
 *            values are frozen and provided here for reference by
 *            humans and bootloader code alike.
 */

#ifndef INCLUDED_NVBOOT_CONFIG_H
#define INCLUDED_NVBOOT_CONFIG_H

#if defined(__cplusplus)
extern "C"
{
#endif


/**
 * Defines the maximum number of device parameter sets in the BCT.
 * The value must be equal to (1 << # of device straps)
 */
#define NVBOOT_BCT_MAX_PARAM_SETS      4

/**
 * Defines the maximum number of SDRAM parameter sets in the BCT.
 * The value must be equal to (1 << # of SDRAM straps)
 */
#define NVBOOT_BCT_MAX_SDRAM_SETS      4

/**
 * Defines the number of bytes provided for the CustomerData field in
 * the BCT.  In this context, "Customer" means a customer of the Boot ROM,
 * namely bootloaders.
 *
 * Note that some of this data has been allocated by other NVIDIA tools
 * and components for their own use.  Please see TBD for further details.
 *
 * The CUSTOMER_DATA_SIZE is set to maximize the use of space within the BCT.
 * The actual BCT size, excluding the Customer data & reserved fields,
 * is 2885 bytes.  This leaves 1192 bytes for customer data (298 32bit words),
 * and the Reserved bytes required are 3 bytes. This brings the total BCT
 * size to 4080 (0xFF0) bytes as required by NVBOOT_BCT_REQUIRED_SIZE below.
 */

/**
 * Defines the number of 32-bit words in the CustomerData area of the BCT.
 */
#define NVBOOT_BCT_CUSTOMER_DATA_WORDS 506

/**
 * Defines the number of bytes in the CustomerData area of the BCT.
 */
#define NVBOOT_BCT_CUSTOMER_DATA_SIZE \
                (NVBOOT_BCT_CUSTOMER_DATA_WORDS * 4)

/**
 * Defines the number of bytes in the Reserved area of the BCT.
 */
#define NVBOOT_BCT_RESERVED_SIZE       3

/**
 * Defines the number of 32-bit words provided in each set of SDRAM parameters
 * for arbitration configuration data.  These values are passed to the
 * bootloader - the Boot ROM does not use the values itself.  Note that this
 * data is part of the SDRAM parameter structure, so there are four sets
 * of this data.
 */
#define NVBOOT_BCT_SDRAM_ARB_CONFIG_WORDS 27

/**
 * Defines the required size of the BCT.  NVBOOT_BCT_REQUIRED_SIZE is set to
 * ensure that the BCT uses the entirety of the pages in which it resides,
 * minus the 16 bytes needed to work around a bug in the reader code.
 */
/**
 * Defines the number of maximum-sized pages needed by the BCT.
 */
#define NVBOOT_BCT_REQUIRED_NUM_PAGES 1
/**
 * Defines the maximum page size needed by the BCT.
 */
#define NVBOOT_BCT_REQUIRED_PAGE_SIZE 6144

/**
 * Defines the required BCT size, in bytes.
 */
#define NVBOOT_BCT_REQUIRED_SIZE \
          ((NVBOOT_BCT_REQUIRED_NUM_PAGES) * (NVBOOT_BCT_REQUIRED_PAGE_SIZE) - 16)


/**
 * Defines the maximum number of bootloader descriptions in the BCT.
 */
#define NVBOOT_MAX_BOOTLOADERS         4

/**
 * Defines the minimum size of a block of storage in the secondary boot
 * device in log2(bytes) units.  Thus, a value of 8 == 256 bytes.
 */
#define NVBOOT_MIN_BLOCK_SIZE_LOG2     8

/**
 * Defines the maximum size of a block of storage in the secondary boot
 * device in log2(bytes) units.  Thus, a value of 23 == 8 mebibytes.
 */
#define NVBOOT_MAX_BLOCK_SIZE_LOG2    23

/**
 * Defines the minimum size of a page of storage in the secondary boot
 * device in log2(bytes) units.  Thus, a value of 8 == 256 bytes.
 */
#define NVBOOT_MIN_PAGE_SIZE_LOG2      8

/**
 * Defines the maximum size of a page of storage in the secondary boot
 * device in log2(bytes) units.  Thus, a value of 13 == 8192 bytes.
 */
#define NVBOOT_MAX_PAGE_SIZE_LOG2     13

/**
 * Defines the maximum page size in bytes (for convenience).
 */
#define NVBOOT_MAX_PAGE_SIZE  (1 << (NVBOOT_MAX_PAGE_SIZE_LOG2))

/**
 * Defines the minimum page size in bytes (for convenience).
 */
#define NVBOOT_MIN_PAGE_SIZE  (1 << (NVBOOT_MIN_PAGE_SIZE_LOG2))

/**
 * Defines the maximum number of blocks to search for BCTs.
 *
 * This value covers the initial block and a set of journal blocks.
 *
 * Ideally, this number will span several erase units for reliable updates
 * and tolerance for blocks to become bad with use.  Safe updates require
 * a minimum of 2 erase units in which BCTs can appear.
 *
 * To ensure that the BCT search spans a sufficient range of configurations,
 * the search block count has been set to 64. This allows for redundancy with
 * a wide range of parts and provides room for greater problems in this
 * region of the device.
 */
#define NVBOOT_MAX_BCT_SEARCH_BLOCKS   64

/**
 * Defines the number of entries (bits) in the bad block table.
 * The consequences of changing its value are as follows.  Using P as the
 * # of physical blocks in the boot loader and B as the value of this
 * constant:
 *    B > P: There will be unused storage in the bad block table.
 *    B < P: The virtual block size will be greater than the physical block
 *           size, so the granularity of the bad block table will be less than
 *           one bit per physical block.
 *
 * 4096 bits is enough to represent an 8MiB partition of 2KiB blocks with one
 * bit per block (1 virtual block = 1 physical block).  This occupies 512 bytes
 * of storage.
 */
#define NVBOOT_BAD_BLOCK_TABLE_SIZE 4096

/**
 * Memory range constants.
 * The range is defined as [Start, End)
 */
/*
 * Note: The following symbolic definitions are consistent with both AP15
 * and AP20.  However, they rely upon constants from project.h, the
 * inclusion of which in the SW tree is undesirable.  Therefore, explicit
 * addresses are used, and these are specific to individual chips or chip
 * families.  The constants here are for AP20.
 *    #define NVBOOT_BL_IRAM_START  (NV_ADDRESS_MAP_IRAM_A_BASE  + 0x8000)
 *    #define NVBOOT_BL_IRAM_END    (NV_ADDRESS_MAP_IRAM_D_LIMIT + 1)
 *    #define NVBOOT_BL_SDRAM_START (NV_ADDRESS_MAP_EMEM_BASE)
 * 
 * As  T30 bootrom needs 8K more IRAM size, NVBOOT_BL_IRAM_START has changed to,
 *    #define NVBOOT_BL_IRAM_START  (NV_ADDRESS_MAP_IRAM_A_BASE  + 0xA000)
 */
/**
 * Defines the starting physical address of IRAM
 */
#define NVBOOT_BL_IRAM_START  (0x40000000 + 0xA000)

/**
 * Defines the ending physical address of IRAM
 */
#define NVBOOT_BL_IRAM_END    (0x4003ffff + 1)

/**
 * Defines the starting physical address of SDRAM
 */
#define NVBOOT_BL_SDRAM_START (0x80000000)

/**
 * Defines the size of IRAM in bytes.
 */
#define NVBOOT_BL_IRAM_SIZE   (NVBOOT_BL_IRAM_END - NVBOOT_BL_IRAM_START)

/**
 * Selection of engines & key slots for AES operations.
 *
 * The SBK key tables are stored in key slots for which read access can 
 * be disabled, but write access cannot be disabled.  Key slots 0 and 1
 * have these characteristics.
 *
 * The SBK key slots are configured for write-only access by the Boot ROM.
 *
 * The bootloader is required to over-write the SBK key slots before
 * passing control to any other code.
 *
 * Either engine can be used for each operation.
 */

/**
 * Defines the engine to use for SBK engine A.  The value is an enumerated
 * constant.
 */
#define NVBOOT_SBK_ENGINEA NvBootAesEngine_A

/**
 * Defines the engine to use for SBK engine B.  The value is an enumerated
 * constant.
 */
#define NVBOOT_SBK_ENGINEB NvBootAesEngine_B

/**
 * Defines the key slot used for encryption with the SBK
 */
#define NVBOOT_SBK_ENCRYPT_SLOT   NvBootAesKeySlot_0
/**
 * Defines the key slot used for decryption with the SBK
 */
#define NVBOOT_SBK_DECRYPT_SLOT   NVBOOT_SBK_ENCRYPT_SLOT

/**
 * The SSK key tables are stored in key slots for which read and/or
 * write access can be disabled.  Key slots 2 and 3 have these
 * characteristics.
 *
 * The SSK key slots are configured for write-only access by the Boot ROM.
 *
 * The SSK key slots can optionally be over-written by the bootloader with
 * any user-defined values.  Regardless, though, the bootloader must ensure
 * that write-access is disabled for these slots (at which time both read-
 * and write-access will be disabled).
 */

/**
 * Defines the engine to use for SSK engine A.  The value is an enumerated
 * constant.
 */
#define NVBOOT_SSK_ENGINEA NvBootAesEngine_A

/**
 * Defines the engine to use for SSK engine B.  The value is an enumerated
 * constant.
 */
#define NVBOOT_SSK_ENGINEB NvBootAesEngine_B

/**
 * Defines the key slot used for encryption with the SSK
 */
#define NVBOOT_SSK_ENCRYPT_SLOT   NvBootAesKeySlot_4
/**
 * Defines the key slot used for decryption with the SSK
 */
#define NVBOOT_SSK_DECRYPT_SLOT   NVBOOT_SSK_ENCRYPT_SLOT

/**
 * Defines the maximum number of fuse words that can be programmed.
 */
#define NVBOOT_FUSE_ARRAY_MAX_WORDS 64

/**
 * Defines the maximum number of commands that can be allowed to be queued.
 * This is as per SATA AHCI spec ver 1.3. Also subject to the fact that queueing
 * is implemented in T30 SATA controller and is taken advantage of in the
 * T30-bootrom SATA driver.
  */
#define NVBOOT_SATA_MAX_COMMANDS_IN_Q    32
#define NVBOOT_SATA_MAX_SUPPORTED_COMMANDS_IN_Q    1

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_NVBOOT_CONFIG_H */
