/**
 * Copyright (c) 2010-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_RECOVERY_UTILS_H
#define INCLUDED_RECOVERY_UTILS_H

#include "nvcommon.h"
#include "nvddk_blockdevmgr_defs.h"
#include "nvaboot.h"
#include "fastboot.h"

typedef enum
{
    CommandType_None,
    CommandType_Update,
    CommandType_Recovery,

    CommandType_Num,

    CommandType_Force32 = 0x7fffffff,
}CommandType;

typedef struct
{
    char command[32];
    char status[32];
    char recovery[1024];
}BootloaderMessage ;

/**
 * Checks the Misc partition for different commands like 'boot-recovery'
 * and 'update'. Also checks the staging partition for 'blob' and updates
 * the respective partition upon the presence of blob.
 *
 * @param hAboot handle to aboot library
 * @param Command Pointer to command enum
 *
 * @retval NvError_Success No Error
 * @retval NvError_InsufficientMemory No free memory available
 * FIXME -- identify failure modes and document error codes
 */
NvError NvCheckForUpdateAndRecovery(NvAbootHandle hAboot, CommandType *Command);

/**
 * Loads the boot.img image from staging partition to memory
 * in case of ODM-secure mode targets after the signature is verified.
 *
 * @param StagingPartitionId staging partition ID
 * @param Image pointer where the boot.img is loaded in the memory
 * @param ImageSize size of the image in the staging partition
 * @param BootImgSignature signature of the boot.img present in the staging
 *
 * @retval NvError_Success No Error
 * @retval NvError_InsufficientMemory No free memory available
 * FIXME -- identify failure modes and document error codes
 */
NvError NvFastbootLoadBootImg(NvU32 StagingPartitionId, NvU8 **Image,
                                NvU32 ImageSize, NvU8 *BootImgSignature);
/**
 * Given the staging partition ID, it will process the blob
 * present in it.
 *
 * @param StagingPartitionId staging partition ID
 *
 * @retval NvError_Success No Error
 * @retval NvError_InsufficientMemory No free memory available
 * FIXME -- identify failure modes and document error codes
 */
NvError NvInstallBlob(char *PartName);

/**
 * Will update the binary from memory to
 * the respective partitions based on the info present in NvPart
 *
 * @param Image the image to be written
 * @param ImageSize size of the image
 * @param NvPart partition information
 *
 * @retval NvError_Success No Error
 * @retval NvError_InsufficientMemory No free memory available
 * FIXME -- identify failure modes and document error codes
 */
NvError NvFastbootUpdateBin(NvU8 *Image, NvU32 ImageSize,
    const PartitionNameMap *NvPart);

/**
 * Writes the specified bootloader message
 *
 * @param hAboot handle to aboot library
 * @param Message the BootloaderMessage to write
 *
 * @retval NvError_Success No Error
 * @retval NvError_InsufficientMemory No free memory available
 * FIXME -- identify failure modes and document error codes
 */
NvError NvWriteCommandsForBootloader(NvAbootHandle hAboot,
                                     BootloaderMessage *Message);

#endif // INCLUDED_RECOVERY_UTILS_H

