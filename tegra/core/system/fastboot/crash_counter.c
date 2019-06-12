/*
 * Copyright (c) 2013, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "crash_counter.h"
#include "nvstormgr.h"
#include "fastboot.h"
#include "recovery_utils.h"
#include "nvos.h"

// Read crash counter from CTR partition.
NvError getCrashCounter(NvU32 *crashCounter)
{
    NvError e = NvSuccess;
    NvU32 bytes;
    NvStorMgrFileHandle hFile = 0;

    // Read current value from partition
    e = NvStorMgrFileOpen("CTR", NVOS_OPEN_READ, &hFile);
    if (e != NvSuccess)
    {
        // Cannot access crash counter partition. Most likely the partition
        // does not exists. If an error is returned the boot process will roll
        // in "recovery" reboots forever. Instead return success. The crash
        // counter logic simply does not work if there is no crash counter partition.
        *crashCounter = 0;
        return(NvSuccess);
    }
    NV_CHECK_ERROR_CLEANUP(
        NvStorMgrFileRead(hFile, crashCounter, sizeof(NvU32), &bytes)
    );
    if (bytes != sizeof(NvU32))
    {
        e = NvError_FileReadFailed;
        goto fail;
    }

fail:
    if (hFile)
        NvStorMgrFileClose(hFile);
    return e;
}

// Write crash counter to the raw CTR partition.
NvError setCrashCounter(NvU32 crashCounter)
{
    NvError e = NvSuccess;
    NvU32 bytes;
    NvStorMgrFileHandle hFile = 0;

    NV_CHECK_ERROR_CLEANUP(
        NvStorMgrFileOpen("CTR", NVOS_OPEN_WRITE, &hFile )
    );

    NV_CHECK_ERROR_CLEANUP(
        NvStorMgrFileWrite(hFile, &crashCounter, sizeof(NvU32), &bytes )
    );
    if (bytes != sizeof(NvU32))
    {
        e = NvError_InvalidSize;
        goto fail;
    }

fail:
    if (hFile)
        NvStorMgrFileClose(hFile);

    return e;
}

// Force a recovery boot by writing the correct string into the bootloader
// message. The bootloader message is stored in the raw MSC partition.
NvError forceRecoveryBoot(NvAbootHandle hAboot)
{
    BootloaderMessage bootMessage;

    // Populate the bootloader message struct
    NvOsStrncpy(bootMessage.command, "boot-recovery", NvOsStrlen("boot-recovery")+1);
    bootMessage.status[0] = (char)0;
    NvOsStrncpy(bootMessage.recovery, "recovery\n--wipe_data\n", NvOsStrlen("recovery\n--wipe_data\n")+1);

    return NvWriteCommandsForBootloader(hAboot, &bootMessage);
}
