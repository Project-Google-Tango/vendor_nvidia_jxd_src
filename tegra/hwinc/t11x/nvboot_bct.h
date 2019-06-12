/*
 * Copyright (c) 2011 - 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * <b>NVIDIA Tegra ODM Kit:
 *         Boot Configuration Table (Tegra APX)</b>
 *
 * @b Description: NvBootConfigTable (BCT) contains the information
 *  needed to load boot loaders (BLs).
 */

/**
 * @defgroup nvbl_bct_ap15 BCT (Tegra APX)
 * @ingroup nvbl_bct_group
 * @{
 *
 * @par Boot Sequence
 *
 * The following is an overview of the boot sequence.
 * -# The Boot ROM (BR) uses information contained in fuses and straps to
 *    determine its operating mode and the secondary boot device from which
 *    to boot. If the recovery mode strap is enabled or the appropriate
 *    AO bit is set, it heads straight to recovery mode.
 *    The BR also intializes the subset of the hardware needed to boot the
 *    device.
 * -# The BR configures the secondary boot device and searches for a valid
 *    Boot Configuration Table (BCT). If it fails to locate one, it enters
 *    recovery mode.
 * -# If the BCT contains SDRAM parameters, the BR configures the SDRAM
 *    controller using the appropriate set.
 * -# If the BCT contains device parameters, the BR reconfigures the
 *    appropriate controller.
 * -# The BCT attempts to load a boot loader (BL), using redundant copies
 *    and failover as needed. The BR enters recovery mode if it cannot load
 *    a valid BL.
 * -# The BR cleans up after itself and hands control over to the BL.
 *
 * <!-- Note: Recovery mode is described in nvboot_rcm.h. -->
 * <!-- During the boot process, the BR records data in the Boot Information
 * Table (BIT). This table provides information to the BL about what
 * transpired during booting, along with a pointer to where a copy of
 * the BCT can be found in memory. Details about the BIT can be found
 * in nvboot_bit.h.
 * -->
 *
 * @par Boot ROM Operating Modes
 *
 * The operating modes of the BR include:
 * - @b NvProduction: This is the mode in which chips are provided to customers
 *   from NVIDIA. In this mode, fuses can still be programmed via recovery
 *    mode. BCTs and BLs are signed with a key of all 0's, but not encrypted.
 * - @b OdmNonSecure: This is the mode in which customers ship products if they
 *   choose not to enable the more stringent security mechanisms. In
 *   this mode, fuses can no longer be programmed. As in NvProduction mode,
 *   BCTs and BLs are signed with a key of all 0's and not encrypted.
 *   This mode is sometimes called OdmProduction.
 * - @b OdmSecure: This is the mode in which customers ship products with the
 *   stricter security measures in force. Fuses cannot be programmed, and
 *   all BCTs and BLs must be signed and encrypted with the secure boot key
 *   (SBK).
 *
 * @par Cryptographic Notes
 *
 * - If a BCT is encrypted, it is encrypted starting from the
 *   NvBootConfigTableRec::RandomAesBlock field and ends at the end of the BCT
 *   (the end of the NvBootConfigTable::Reserved area).
 * - If a BL is encrypted, the entire BL image, including any padding, is
 *   encrypted.
 * - Signatures are computed as a CMAC hash over the encrypted data.
 * - All cryptographic operations use 128-bit AES in CBC mode w/an IV of 0's.
 *
 * @par Requirements for a Good BCT
 *
 * To be used by the BR, the BCT's CryptoHash must match the hash value
 * computed while reading the BCT from the secondary boot device.
 *
 * For secondary boot devices that do not naturally divide storage into pages
 * and blocks, suitable values have been chosen to provide a consistent model
 * for BCT and BL loading. For eMMC devices, the page size is fixed at 512
 * bytes and the block size is 4096 bytes.
 *
 * <!-- Additional requirements for BCTs created from scratch are:
 *  - The BootDataVersion must match the BR's data structure version number.
 *  - The block and page sizes must lie within the allowed range.
 *  - The block and page sizes must match the sizes used by the device manager
 *    to talk to the device.
 *  - The partition size must be a multiple of the block size.
 *  - The number of SDRAM and device parameter sets must be within range.
 *  - The block size used by the bad block table must be the BCT's block size.
 *  - The block size must be <= the virtual block size used by the
 *    bad block table.
 *  - The number of entries used within the bad block table must fit within
 *    the space available in the table.
 *  - The number of entries used must be equal to the number of virtual blocks
 *    that fit within the partition size.
 *  - The number of BLs present must fit within the available table space.
 *  - For each BL in the table:
 *     -# The starting page must fit within a block.
 *     -# The length of the BL > 0.
 *     -# The BL must fit within the partition.
 *     -# The entry point must lie within the BL.
 *  - The \c Reserved field must contain the padding pattern, which is one byte
 *    of 0x80 followed by bytes of 0x00.
 * -->
 * @par Boot ROM Search for a Good BCT
 *
 * After configuring the hardware to read from the secondary boot device,
 * the BR commences a search for a valid BCT. In the descriptions that
 * follow, the term "slot" refers to a potential location of a BCT in a block.
 * A slot is the smallest integral number of pages that can hold a BCT.
 * Thus, every BCT begins at the start of a page and may span multiple pages.
 *
 * The search sequence is:
 * <pre>
 *    Block 0, Slot 0
 *    Block 0, Slot 1
 *    Block 1, Slot 0
 *    Block 1, Slot 1
 *    Block 1, Slot 2
 *    . . .
 *    Block 1, Slot N
 *    Block 2, Slot 0
 *    . . .
 *    Block 2, Slot N
 *    . . .
 *    Block 63, Slot N
 * </pre>
 *
 * A few points worthy of note:
 * - Block 0 is treated differently from the rest. In some storage devices,
 *   this block has special properties, such as being guaranteed to be good
 *   from the factory.
 * - The remaining blocks that are searched are journal blocks. These are
 *   backups which provide a means to boot the system in the presence of
 *   unexpected failures or interrupted device updates.
 * - The search within a journal block ends as soon as a bad BCT or a read
 *   error is found.
 * - Not all of the journal blocks need to contain BCTs. If the BR reads
 *   non-BCT data, it should fail to validate.
 * - The search terminates when:
 *    -# A good BCT is found in either of the slots in Block 0.
 *    -# A good BCT is found in a journal block and either the end of the
 *       block is reached or an error (validation failure or read error)
 *       occurs. The last good BCT in the journal block is used.
 *
 * Once a good BCT has been located, the BR proceeds with the boot sequence.
 *
 * <!-- Details of the SDRAM and device parameters are contained within their
 * respective header files.
 * -->
 *
 * The BR attempts to load each BL in the order they appear in the BootLoader
 * table (which is an array of \c NvBootLoaderInfo structures) until locating a
 * good one. A BL is good if it fits within the
 * destination memory area and passes the signature check.
 *
 * The BR begins reading a BL from NvBootLoaderInfoRec::StartPage within
 * NvBootLoaderInfoRec::StartBlock. It continues to read pages sequentially
 * from this point, skipping over known bad blocks. Read failures cause the
 * BR to use data from the redundant copies in an effort to assemble a
 * complete, good BL.
 *
 * By default, the BR will only load BLs from the first generation it finds.
 * A generation is a set of BLs with the same version number. If FailBack
 * is enabled via an AO bit, the BR will continue to load BLs from older
 * generations if it is unable to successfully load a BL from the newest
 * generation. The age of a generation is implied by the order of entries
 * in the BootLoader table--smaller indices indicate newer generations.
 * All BLs of the same generation must occupy adjacent entries in the
 * BootLoader table.
 */

#ifndef INCLUDED_NVBOOT_BCT_H
#define INCLUDED_NVBOOT_BCT_H

#include "nvcommon.h"
#include "nvboot_config.h"
#include "nvboot_badblocks.h"
#include "nvboot_devparams.h"
#include "nvboot_fuse.h"
#include "nvboot_hash.h"
#include "nvboot_sdram_param.h"
#include "nvboot_se_rsa.h"

#if defined(__cplusplus)
extern "C"
{
#endif


/**
 * Stores information needed to locate and verify a boot loader.
 *
 * There is one \c NvBootLoaderInfo structure for each copy of a BL stored on
 * the device.
 */
typedef struct NvBootLoaderInfoRec
{
    /// Specifies a version number for the BL. The assignment of numbers is
    /// arbitrary; the numbers are only used to identify redundant copies
    /// (which have the same version number) and to distinguish between
    /// different versions of the BL (which have different numbers).
    NvU32      Version;

    /// Specifies the first physical block on the secondary boot device
    /// that contains the start of the BL. The first block can never be
    /// a known bad block.
    NvU32      StartBlock;

    /// Specifies the page within the first block that contains the start
    /// of the BL.
    NvU32      StartPage;

    /// Specifies the length of the BL in bytes. BLs must be padded
    /// to an integral number of 16 bytes with the padding pattern.
    /// @note The end of the BL cannot fall within the last 16 bytes of
    /// a page. Add another 16 bytes to work around this restriction if
    /// needed.
    NvU32      Length;

    /// Specifies the starting address of the memory region into which the
    /// BL will be loaded.
    NvU32      LoadAddress;

    /// Specifies the entry point address in the loaded BL image.
    NvU32      EntryPoint;

    /// Specifies an attribute available for use by other code.
    /// Not interpreted by the Boot ROM.
    NvU32      Attribute;

    /// Specifies the AES-CMAC MAC or RSASSA-PSS signature of the BL.
	NvBootObjectSignature Signature;
} NvBootLoaderInfo;

/**
 * Identifies the types of devices from which the system booted.
 * Used to identify primary and secondary boot devices.
 * Note that these no longer match the fuse API device values (for
 * backward compatibility with AP15).
 */
typedef enum
{
    /// Specifies a default (unset) value
    NvBootDevType_None = 0,

    /// Specifies NAND
    NvBootDevType_Nand,

    /// Specifies an alias for 8-bit NAND
    NvBootDevType_Nand_x8 = NvBootDevType_Nand,

    /// Specifies SNOR
    NvBootDevType_Snor,

    /// Specifies an alias for NOR
    NvBootDevType_Nor = NvBootDevType_Snor,

    /// Specifies SPI NOR
    NvBootDevType_Spi,

    /// Specifies SDMMC (either eMMC or eSD)
    NvBootDevType_Sdmmc,

    /// Specifies internal ROM (i.e., the BR)
    NvBootDevType_Irom,

    /// Specifies UART (only available internal to NVIDIA)
    NvBootDevType_Uart,

    /// Specifies USB (i.e., RCM)
    NvBootDevType_Usb,

    /// Specifies 16-bit NAND
    /// Note: Not used in AP20 - just _Nand
    NvBootDevType_Nand_x16,

    /// Specifies USB3 boot interface
    NvBootDevType_Usb3,

    NvBootDevType_Max,

    NvBootDevType_Force32 = 0x7FFFFFFF
 } NvBootDevType;

/**
 * Contains the information needed to load BLs from the secondary boot device.
 *
 * - Supplying NumParamSets = 0 indicates not to load any of them.
 * - Supplying NumDramSets  = 0 indicates not to load any of them.
 * - The \c RandomAesBlock member exists to increase the difficulty of
 *   key attacks based on knowledge of this structure.
 */
typedef struct NvBootConfigTableRec
{
    /// *** UNSIGNED SECTION OF THE BCT *** ///
    ///
    /// IMPORTANT NOTE: If the start of the unsigned section changes from
    ///                 RandomAesBlock to some other starting point,
    ///                 other parts of Boot ROM must be updated!
    ///                 See SignatureOffset in function ReadOneBct
    ///                 in nvboot_bct.c, as well as the compile time
    ///                 assert at around line 59 nvboot_bct.c.
    ///                 (This is NOT a comprehensive list).
    ///
    /// IMPORTANT NOTE 2: The size of the unsigned section must be a multiple
    ///                   of the AES block size, to maintain compatibility
    ///                   with the nvboot_reader function LaunchCryptoOps!
    ///
    /// Specifies the bad block table, which is defined in nvboot_badblocks.h.
    NvBootBadBlockTable BadBlockTable;

    /// Sepcifies the RSA public key's modulus
    NvBootRsaKeyModulus Key;

    /// Specifies the AES-CMAC MAC or RSASSA-PSS signature for the rest of the BCT structure
    NvBootObjectSignature Signature;

    /// Specifies a region of data available to customers of the BR.
    /// This data region is primarily used by a manufacturing utility
    /// or BL to store useful information that needs to be
    /// shared among manufacturing utility, BL, and OS image.
    /// BR only provides framework and does not use this data
    /// @note Some of this space has already been allocated for use
    /// by NVIDIA.
    /// Information currently stored in the \c CustomerData[] buffer is
    /// defined below.
    /// @note Some of the information mentioned shall be deprecated
    /// or replaced by something else in future releases
    ///
    /// -# Start location of OS image (physical blocks). Size:- NvU32
    ///    OS image is written from block boundary.
    /// -# Length of OS image. Size:- NvU32
    /// -# OS Flavor: wince or winwm (windows mobile). Size:-NvU32
    ///    wince type image is a raw binary
    ///    winwm has different image layout (".dio" format)
    /// -# Pointer to the bad block table for complete nand media. Size:- NvU32
    /// -# Information about how many columns (banks) are used for
    ///    NAND interleave operations. Size:- NvU8
    /// -# Pointer to DRM device certificate location. Size:-NvU32
    /// -# Pointer to secure clock information. Size:- NvU32
    /// -# \a custopt data filed. Size: NvU32
    ///    RM allows ODM adaptations and ODM query implementations
    ///    to read this value at runtime and use it for various useful
    ///    features.
    ///    For example: use of single BSP image that supports multiple product
    ///    SKUs.
    /// @note The storage space is much larger for AP20 than AP15 or AP16.
    ///
    /// The last few words of this field hold a struct NvBctAuxInternalData.
    NvU8                CustomerData[NVBOOT_BCT_CUSTOMER_DATA_SIZE];

    /// *** START OF SIGNED SECTION OF THE BCT *** ///
    ///
    /// Specifies a chunk of random data.
    NvBootHash          RandomAesBlock;

    /// Specifies the Unique ID / ECID of the chip that this BCT is specifically
    /// generated for. This field is required if SecureJtagControl == NV_TRUE.
    /// It is optional otherwise. This is to prevent a signed BCT with
    /// SecureJtagControl == NV_TRUE being leaked into the field that would
    /// enable JTAG debug for all devices signed with the same private RSA key.
    NvBootECID          UniqueChipId;

    /// Specifies the version of the BR data structures used to build this BCT.
    /// \c BootDataVersion must match the version number in the BR.
    NvU32               BootDataVersion;

    /// Specifies the size of a physical block on the secondary boot device
    /// in log2(bytes).
    NvU32               BlockSizeLog2;

    /// Specifies the size of a page on the secondary boot device
    /// in log2(bytes).
    NvU32               PageSizeLog2;

    /// Specifies the size of the boot partition in bytes.
    /// Used for internal error checking; BLs must fit within this region.
    NvU32               PartitionSize;

    /// Specifies the number of valid device parameter sets provided within
    /// this BCT. If the device straps are left floating, the same parameters
    /// should be replicated to all NVBOOT_BCT_MAX_PARAM_SETS sets.
    NvU32               NumParamSets;


    // Specifies the type of device for parameter set DevParams[i]
    NvBootDevType       DevType[NVBOOT_BCT_MAX_PARAM_SETS];

    /// Specifies the device parameters with which to reinitialize the
    /// secondary boot device controller. The device straps index into this
    /// table. The definition of \c NvBootDevParams is contained within
    /// nvboot_devparams.h and the specific device nvboot_*_param.h files.
    NvBootDevParams     DevParams[NVBOOT_BCT_MAX_PARAM_SETS];

    /// Specifies the number of valid SDRAM parameter sets provided within
    /// this BCT. If the SDRAM straps are left floating, the same parameters
    /// should be replicated to all NVBOOT_BCT_MAX_SDRAM_SETS sets.
    NvU32               NumSdramSets;

    /// Specifies the SDRAM parameters with which to initialize the SDRAM
    /// controller. The SDRAM straps index into this table. The definition
    /// of NvBootDevParams is contained within nvboot_sdram_param.h.
    NvBootSdramParams   SdramParams[NVBOOT_BCT_MAX_SDRAM_SETS];

    /// Specifies the number of BLs described in the BootLoader table.
    NvU32               BootLoadersUsed;

    /// Specifies the information needed to locate and validate each BL.
    /// The BR uses entries 0 through BootLoadersUsed-1.
    NvBootLoaderInfo    BootLoader[NVBOOT_MAX_BOOTLOADERS];

    /// Specifies whether FailBack should be used when looking for a good BL.
    NvBool              EnableFailBack;

    /// Specify whether or not to enable JTAG access when the JTAG disable fuse
    /// has not been burned.
    /// SecureJtagControl = NV_FALSE (0) = Disable JTAG access.
    /// SecureJtagControl = NV_TRUE (1) = Enable JTAG access.
    NvBool              SecureJtagControl;

    /// Specifies a reserved area at the end of the BCT that must be filled
    /// with the padding pattern.
    NvU8                Reserved[NVBOOT_BCT_RESERVED_SIZE];
} NvBootConfigTable;

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_NVBOOT_BCT_H */
