/*
 * Copyright (c) 2009-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

/**
 * @file
 * <b>NVIDIA Aboot Interface: Boot Loader APIs</b>
 *
 * @b Description: Defines a boot loader interface.
 */

#ifndef NVABOOT_INCLUDED_H
#define NVABOOT_INCLUDED_H

#include "nvcommon.h"
#include "nverror.h"
#include "nvrm_memmgr.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * This header is split into sections: general, utility functions, and
 * helper functions.
 */

/**
 * @defgroup nvbdk_boot Aboot Boot Loader APIs
 *
 * This defines a boot loader interface.
 * @ingroup nvbdk_modules
 * @{
 */

/**
 * Compressed kernel image will be loaded at this address.
 */
#define ZIMAGE_START_ADDR (0xA00000UL)  /**< Offset @ 10 MB. */

/**
 * Size of the memory reserved for loading compressed kernel image. Because the
 * decompressor of the kernel decompresses the kernel image and uses the memory
 * from the beginning, this allocation has 32 MB to spare. With this margin
 * we avoid disturbing the already loaded ramdisk @32 MB offset.
 */
#define AOS_RESERVE_FOR_KERNEL_IMAGE_SIZE (0x1600000) /**< 22 MB. */
#define RAMDISK_MAX_SIZE                  (0x1400000) /**< 20 MB. */
#define DTB_MAX_SIZE                      (0x400000)  /**< 4 MB. */


/** @name General APIs
 */
/*@{*/


/**
 * Opaque handle to Aboot context.
 */
typedef struct NvAbootRec *NvAbootHandle;

/* Forward declaration */
struct NvBctAuxInfoRec;

/**
 * Defines DRAM partition types.
 *
 * The DRAM will be partitioned into one or more areas, each managed by
 * independent software components.
 */
typedef enum
{
    /// Primary system RAM aperture, managed by the operating system
    NvAbootDramPart_Ram = 0,

    /// Extended, low-address system RAM aperture, managed by the
    /// operating system.
    NvAbootDramPart_ExtendedLow = 1,

    NvAbootDramPart_Num,

    NvAbootDramPart_Force32 = 0x7fffffffUL,
} NvAbootDramPartType;

#define WB0_CODE_LENGTH_ALIGNMENT 16
#define WB0_CODE_ADDR_ALIGNMENT 4096

/**
 * Defines a DRAM partition
 */

typedef struct NvAbootDramPartRec
{
    /// Starting physical address of the DRAM partition
    NvU64 Bottom;
    /// Ending physical address of the DRAM partition
    NvU64 Top;
    /// Partition type
    NvAbootDramPartType Type;
} NvAbootDramPart;

#ifdef LPM_BATTERY_CHARGING
/**
 * Defines low power mode battery charging actions.
 */
typedef enum
{
    /**
     * Lowers the CPU source clock frequency
     * and disables the peripheral clocks.
     */
    NvAbootLpmId_GoToLowPowerMode,

    /**
     * Restores the CPU source clock frequency
     * and enables the disabled peripheral clocks.
     */
    NvAbootLpmId_ComeOutOfLowPowerMode,

} NvAbootLpmId;

/**
 * Defines clock enable flags for low power mode battery charging.
 */
typedef enum
{
    /**
     * Enables all the peripheral clocks.
     */
    NvAbootClocksId_EnableClocks,
    /**
     * Disables all the peripheral clocks.
     */
    NvAbootClocksId_DisableClocks,
    /**
     * Enables the clocks related to display.
     */
    NvAbootClocksId_EnableDisplayClocks,
    /**
     * Disables the clocks related to display.
     */
    NvAbootClocksId_DisableDisplayClocks,
}NvAbootClocksId;
#endif

/**
 * Defines the memory layout type to be queried for its base/size
 * (used with ::NvAbootMemLayoutBaseSize).
 */
typedef enum
{
    /** Specifies primary memory. */
    NvAbootMemLayout_Primary = 0,

    /** Specifies memory beyond primary partition. */
    NvAbootMemLayout_Extended,

    /** Specifies SecureOS base/size. */
    NvAbootMemLayout_SecureOs,

    /** Specifies VPR base/size. */
    NvAbootMemLayout_Vpr,

    /** Specifies TSEC base/size. */
    NvAbootMemLayout_Tsec,

    /** Specifies Debug base/size. */
    NvAbootMemLayout_Debug,

    /** Specifies XUSB base/size. */
    NvAbootMemLayout_Xusb,

    /** Specifies LP0 code base/size. */
    NvAbootMemLayout_lp0_vec,

    /** Specifies BBC IPC base/size. */
    NvAbootMemLayout_BBC_IPC,

    /** Specifies BBC PVT base/size. */
    NvAbootMemLayout_BBC_PVT,

    /** Specifies RAM dump base/size. */
    NvAbootMemLayout_RamDump,

    /** NCK base/size. */
    NvAbootMemLayout_Nck,

    /** Specifies the number of entries. */
    NvAbootMemLayout_Num,
} NvAbootMemLayoutType;

/**
 * Defines the different possible BBC aperture sizes.
 */
typedef enum
{
    /** Specifies 8 MB aperture.  */
    NvBBCApertureSize_P8MB = 0x00800000,

    /** Specifies 16 MB aperture.  */
    NvBBCApertureSize_P16MB = 0x01000000,

    /** Specifies 32 MB aperture.  */
    NvBBCApertureSize_P32MB = 0x02000000,

    /** Specifies 64 MB aperture.  */
    NvBBCApertureSize_P64MB = 0x04000000,

    /** Specifies 0 MB aperture. */
    NvBBCApertureSize_P0MB = 0x00000000,

    /** Ignore -- Forces compilers to make 32-bit enums. */
   NvBBCApertureSize_Force32 = 0x7fffffffUL,
}NvBBCApertureSize;

/**
 * Defines the register values for different possible BBC aperture sizes.
 */
typedef enum
{
    /** Specifies 8 MB aperture.  */
    NvBBCApertureRegValue_P8MB = 0,

    /** Specifies 16 MB aperture.  */
    NvBBCApertureRegValue_P16MB,

    /** Specifies 32 MB aperture.  */
    NvBBCApertureRegValue_P32MB,

    /** Specifies 64 MB aperture.  */
    NvBBCApertureRegValue_P64MB,

    /** Specifies 0 MB aperture. */
    NvBBCApertureRegValue_P0MB = 7,

    /** Ignore -- Forces compilers to make 32-bit enums. */
   NvBBCApertureRegValue_Force32 = 0x7fffffffUL,
}NvBBCApertureRegValue;

/* Memory Layout DB: tracks the base/size of the carveouts
 * (for example, for SecureOS and VPR).
 */
typedef struct NvAbootLayoutRec
{
    NvU64  Base;
    NvU64 Size;
} NvAbootMemLayoutRec;

/*@}*/
/** @name Utility Functions
 * System utility functions for initializing and shutting down boot loader
 * services, and for jumping to OS images.
*/
/*@{*/

/**
 * Initializes Aboot. This should be the first function called by Aboot loader.
 *
 * @param hAboot A pointer to Aboot handle to use for subsequent Aboot APIs,
 *   or NULL if an error occurred.
 * @param EnterNv3pServer A pointer to a value indicating whether the boot loader
 *   should execute the Nv3p server code for recovery mode or jump to normal
 *   execution.  @see NvAboot3pServer
 * @param BootDev A pointer to the secondary boot device type. This is one
 *   of the NvBootDevType enumerants, cast into an NvU32.
 *
 * @retval NvSuccess If successful, or the appropriate error code.
 */
NvError NvAbootOpen(
    NvAbootHandle *hAboot,
    NvU32 *EnterNv3pServer,
    NvU32 *BootDev);

/**
 * Used to query the memory partition layout.
 *
 * @param hAboot Aboot handle
 * @param pDramPart Pointer to return structure containing the DRAM partition
 *   parameters
 * @param PartNumber Partition number to query; partition numbers start at 0
 *   and end at a system-dependent number.
 *
 * @retval NV_TRUE if PartNumber exists and the return data is valid;
 *   NV_FALSE otherwise.
 */
NvBool NvAbootGetDramPartitionParameters(
    NvAbootHandle hAboot,
    NvAbootDramPart *pDramPart,
    NvU32 PartNumber);

/**
 * Returns the start and size of the named partition that was specified in
 * the NvFlash configuration file.
 *
 * @param hAboot Aboot handle.
 * @param NvPartName Name of the partition to query.
 * @param StartPhysicalSector Start address of the partition, in physical sectors.
 * @param NumLogicalSectors Size of the partition, in logical sectors.
 * @param SectorSize Size of each logical sector, in bytes.
 *
 * @retval NvSuccess If successful, or the appropriate error code.
 */

NvError NvAbootGetPartitionParameters(
    NvAbootHandle hAboot,
    const char *NvPartName,
    NvU64 *StartPhysicalSector,
    NvU64 *NumLogicalSectors,
    NvU32 *SectorSize);

/**
 * Queries the OS view of the memory layout (in other words,
 * for a particular memory \a Type, what is its memory \a Base and \a Size).
 *
 * @param hAboot Aboot handle.
 * @param Type Memory type of this query.
 * @param Base Optional pointer to the base of memory type.
 * @param Size Optional pointer to the size of memory type.
 *
 * @retval NV_TRUE If \a Type is a valid memory type, or NV_FALSE otherwise.
 */
NvBool NvAbootMemLayoutBaseSize(
    NvAbootHandle hAboot,
    NvAbootMemLayoutType Type,
    NvU64 *Base,
    NvU64 *Size);

/**
 * This function should be the last function called in any aboot boot loader.
 * It will safely copy one or more images located in OS-allocated virtual
 * addresses to user-specified physical addresses, and then jump to a
 * user-specified physical address with user-specified register values to
 * start the kernel.
 *
 * At most \a NumImages and \a NumKernelRegisters (implementation defined to
 * 4) may be specified and copied using this interface. Furthermore,
 * the destination addresses must not overlap with the boot loader's code
 * and data sections, and sufficient carveout memory must be available to
 * resolve circular copy dependencies, if any image's source data overlaps
 * another image's destination address.
 *
 * If successful, this function does not return; instead, it jumps to a
 * user-provided kernel start address. As a side effect, it disables all
 * caches, virtual-address translation, and interrupts.
 *
 * @param hAboot Aboot instance handle.
 * @param pDsts A pointer to an array of physical addresses for the copy
 *     destinations.
 * @param pSrcs A pointer to an array of virtual addresses for the source data.
 * @param pSizes A pointer to an array specifying the size of each buffer.
 * @param NumImages Number of entries in the \a pDsts, \a pSrcs, and \a pSizes arrays.
 * @param KernelStart Kernel entry point address.
 * @param pKernelRegisters A pointer to an array of values to load into the
 *     CPU registers prior to jumping to \a KernelStart. The first entry
 *     in this array is loaded into r0, the second into r1, and so on.
 * @param NumKernelRegisters Number of entries in the \a pKernelRegisters array.
 *
 * @retval NvSuccess If successful, or the appropriate error code.
 */
NvError NvAbootCopyAndJump(
    NvAbootHandle hAboot,
    NvUPtr       *pDsts,
    void        **pSrcs,
    NvU32        *pSizes,
    NvU32         NumImages,
    NvUPtr        KernelStart,
    NvU32        *pKernelRegisters,
    NvU32         NumKernelRegisters);

/*@}*/
/** @name Helper Functions
 * Helper functions for reading and writing data to NvBasicFileSystem-formatted
 * partitions on any mass storage medium
*/
/*@{*/

/**
 * Allocates a buffer of size \a BytesToRead for the named partition, and reads
 * \a BytesToRead bytes starting from \a ByteOffset of partition into it.
 *
 * @param hAboot Aboot instance handle.
 * @param NvPartitionName A pointer to the name of the partition to read.
 * @param ByteOffset From where to start reading, in bytes.
 * @param BytesToRead Size to read, in bytes.
 * @param Buff A pointer to a buffer of size \a BytesToRead.
 *      It is the caller's responsibility to allocate and free this buffer.
 * @retval NvSuccess If successful, or the appropriate error code.
 */

NvError NvAbootBootfsReadNBytes(
    NvAbootHandle hAboot,
    const char   *NvPartitionName,
    NvS64         ByteOffset,
    NvU32         BytesToRead,
    NvU8         *Buff);

/**
 * Allocates a buffer large enough for the named partition, and reads the
 * entire partition into it.
 *
 * @param hAboot Aboot instance handle.
 * @param NvPartitionName Name of the partition to read.
 * @param pBuff A pointer to a buffer that will be allocated by the function.
 *      Freeing this buffer is the caller's responsibility.
 * @param pSize Size of the buffer that was allocated, in bytes.
 * @retval NvSuccess If successful, or the appropriate error code.
 */
NvError NvAbootBootfsRead(
    NvAbootHandle hAboot,
    const char   *NvPartitionName,
    NvU8        **pBuff,
    NvU32        *pSize);

/**
 * Writes a buffer to a named partition in mass storage.
 *
 * @param hAboot Aboot instance handle.
 * @param NvPartitionName Name of the partition to write.
 * @param pBuff A pointer to the array of data to write.
 * @param Size Size of the data that will be written, in bytes.
 * @retval NvSuccess If successful, or the appropriate error code.
 */
NvError NvAbootBootfsWrite(
    NvAbootHandle hAboot,
    const char   *NvPartitionName,
    const NvU8   *pBuff,
    NvU32         Size);

/**
 * Formats a given partition in compliance with the underlying FS.
 *
 * @param NvPartitionName Name of the partition to format.
 * @retval NvSuccess If successful, or the appropriate error code.
 */
NvError NvAbootBootfsFormat(
    const char   *NvPartitionName);

/**
 * Fills a storage manager partition with empty data (0xff, assuming this
 * is the clear value for the underlying storage medium).
 *
 * @param hAboot Aboot handle returned by NvAbootOpen().
 * @param NvPartitionName Name of the partition to erase.
 * @retval NvSuccess If successful, or the appropriate error code.
 */
NvError NvAbootRawErasePartition(
    NvAbootHandle hAboot,
    const char   *NvPartitionName);

/**
 * Writes a raw image to a specified Nv Storage Manager partition
 * on the boot device. Partitions do not need to be erased prior to calling
 * this function. This function does not handle bad blocks, so it should only
 * be used for mass storage devices that internally manage bad blocks, such as
 * MMC. If the length or offset of the specified data is not aligned to the
 * underlying storage device's sector size, this function performs a
 * read-modify-write operation to ensure that existing data is not corrupted.
 *
 * @param hAboot Aboot handle returned by NvAbootOpen().
 * @param NvPartitionName Name of the partition to write
 * @param pBuff A pointer to an array of bytes to be stored to the partition.
 * @param BuffSize Length of the data stored in pBuff, in bytes.
 * @param PartitionOffset Offset in bytes from the start of the partition to
 *     to write this data.
 * @retval NvSuccess If successful, or the appropriate error code.
 */
NvError NvAbootRawWritePartition(
    NvAbootHandle hAboot,
    const char   *NvPartitionName,
    const NvU8   *pBuff,
    NvU64         BuffSize,
    NvU64         PartitionOffset);

/**
 * Writes an master boot record (MBR) to the device. The MBR consists of a
 * table of partition parameters.
 *
 * @param hAboot Aboot handle returned by NvAbootOpen().
 * @retval NvSuccess If successful, or the appropriate error code.
 */
NvError NvAbootRawWriteMBRPartition(
    NvAbootHandle hAboot);

/**
 * Implements NvFlash recovery mode.
 *
 * @param hAboot Aboot handle returned by NvAbootOpen().
 * @retval NvSuccess If successful, or the appropriate error code.
 */
NvError NvAboot3pServer(NvAbootHandle hAboot);

/**
 * Signs the warm bootloader appropriately and
 * stores the signed version at SegAddress
 *
 * @param hAboot Aboot handle returned by NvAbootOpen().
 * @retval NvSuccess If successful, or the appropriate error code.
 */
NvError NvAbootInitializeCodeSegment(NvAbootHandle hAboot);

/**
 * Destroys the original plain LP0 exit code.
 */
void NvAbootDestroyPlainLP0ExitCode(void);

#ifdef LPM_BATTERY_CHARGING
/**
 * Puts the device into low power mode, or
 * if already in low power mode brings out of it.
 *
 * @param Id Specifies to enter low power mode
 * or come out of low power mode.
 */
NvError NvAbootLowPowerMode(NvAbootLpmId Id, ...);

/**
 * Enables/Disables the clocks.
 *
 * @param Id Specifies the type of clocks to be
 * enabled/disabled.
 * @param hRm A handle for NvRM device.
 */
void NvAbootClocksInterface(NvAbootClocksId Id, NvRmDeviceHandle hRm);
#endif

/**
 * Reboots the processor.
 *
 * @param hAboot Aboot handle returned by NvAbootOpen().
 */
void NvAbootReset(NvAbootHandle hAboot);

/**
 * Returns the active cluster.
 *
 * @param hAboot Aboot handle returned by NvAbootOpen().
 * @retval 0 Indicates the G-cluster is active.
 * @retval 1 Indicates the LP-cluster is active.
 */
NvBool NvAbootActiveCluster(NvAbootHandle hAboot);

/**
 * Sets the required configuration for the cluster switch.
 *
 * @param hAboot Aboot handle returned by NvAbootOpen().
 * @param target_cluster Specifies which cluster to configure.
 *        0 selects the G-cluster, and 1 selects the LP-cluster.
 * @retval NvSuccess If successful, or the appropriate error code.
 */
NvError NvAbootConfigureClusterSwitch(NvAbootHandle hAboot, NvBool target_cluster);

/**
 * Powers off the device.
 *
 * @param hAboot Aboot handle returned by NvAbootOpen().
 */
void NvAbootHalt(NvAbootHandle hAboot);

/**
 * Reads the warmboot parameters DRAM address, block size, number of set of SDRAM params.
 *
 * @param Wb0Address A pointer to the chip-specific warmboot parameters.
 * @param Instances A pointer to the number of sets of warmboot params available.
 * @param BlockSize A pointer to the block size of one set of warmboot parameters.
 * @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
NvBool NvAbootGetWb0Params(NvU32 *Wb0Address,
                  NvU32 *Instances, NvU32 *BlockSize);

/**
 * Enables fuse mirroring.
 *
 * @param hAboot Aboot handle returned by NvAbootOpen().
 */
void NvAbootEnableFuseMirror(NvAbootHandle hAboot);

/**
 * Disables usb interrupts.
 *
 * @param hAboot Aboot handle returned by NvAbootOpen().
 */
void NvAbootDisableUsbInterrupts(NvAbootHandle hAboot);

/**
 * Disables interrupts and flushes caches.
 */
void NvAbootPrivSanitizeUniverse(NvAbootHandle hAboot);

NvError NvAbootGetPartitionParametersbyId(
    NvAbootHandle hAboot,
    NvU32      PartId,
    NvU64      *StartPhysicalSector,
    NvU64      *NumPhysicalSectors,
    NvU32      *SectorSize);

/**
 * Obtains the aux info out of the BCT structure.
 */
NvError AbootPrivGetOsInfo(struct NvBctAuxInfoRec *pAuxInfo);

/**
 *  This macro can be used to insert an infinite-loop with a unique identifier
 *  value, in the event of boot loader errors.
 */
#define NV_ABOOT_BREAK() \
    do {                                                                    \
        NvU32 _x = (NvU32)__LINE__;  \
        volatile NvU32 *_px = (volatile NvU32 *) &_x;                       \
        while (*_px == *_px) { }                                            \
    } while (1);
/*@}*/

#ifdef __cplusplus
}
#endif

/** @} */
#endif
