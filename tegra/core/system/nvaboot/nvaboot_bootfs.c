/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 *
 * nvaboot_bootfs.c
 *
 *   Utility functions for operating on bootfs (NVIDIA Basic file system)
 *   partitions stored on any type of mass storage.
 */

#include "nvcommon.h"
#include "nvaboot.h"
#include "nvaboot_private.h"
#include "nvpartmgr.h"
#include "nvstormgr.h"
#include "nvos.h"
#include "nvsystem_utils.h"

NvError NvAbootBootfsReadNBytes(
    NvAbootHandle hAboot,
    const char  *NvPartitionName,
    NvS64       ByteOffset,
    NvU32       BytesToRead,
    NvU8       *Buff)
{
    NvFileStat          FileStat;
    NvStorMgrFileHandle hFile = NULL;
    NvU32               BytesRead;
    NvError             e = NvSuccess;

    if (!hAboot || !NvPartitionName || !Buff || !BytesToRead)
        return NvError_BadParameter;

    NvOsMutexLock(hAboot->hStorageMutex);

    NV_CHECK_ERROR_CLEANUP(NvStorMgrFileQueryStat(NvPartitionName, &FileStat));

    if (!FileStat.Size)
    {
        e = NvError_FileOperationFailed;
        goto fail;
    }

    if (!(((ByteOffset >= 0) && (BytesToRead > 0)) &&
        ((ByteOffset + (NvU64)BytesToRead) <= FileStat.Size)))
    {
        e = NvError_BadParameter;
        goto fail;
    }

    NV_CHECK_ERROR_CLEANUP(
        NvStorMgrFileOpen(NvPartitionName, NVOS_OPEN_READ, &hFile)
    );

    NV_CHECK_ERROR_CLEANUP(NvStorMgrFileSeek(hFile, ByteOffset, NvOsSeek_Set));

    NV_CHECK_ERROR_CLEANUP(
        NvStorMgrFileRead(hFile, Buff, BytesToRead, &BytesRead)
    );

    if (BytesRead != BytesToRead)
    {
        e = NvError_EndOfFile;
        goto fail;
    }

    NV_CHECK_ERROR_CLEANUP(NvStorMgrFileClose(hFile));

fail:
    NvOsMutexUnlock(hAboot->hStorageMutex);
    return e;
}

/**
 * NvAbootBootfsRead
 *   Reads the partition named NvPartitionName in the NVFlash partition table.
 *   Allocates memory to read the partition
 */

NvError NvAbootBootfsRead(
    NvAbootHandle hAboot,
    const char   *NvPartitionName,
    NvU8        **pBuff,
    NvU32        *pSize)
{
    NvFileStat          FileStat;
    NvStorMgrFileHandle hFile = NULL;
    NvU32               PartitionId;
    NvU32               BytesRead;
    NvU8               *pRead = NULL;
    NvError             e, ec;

    if (!hAboot || !NvPartitionName || !pBuff || !pSize)
        return NvError_BadParameter;

    if (*pBuff != NULL)
        return NvError_BadParameter;

    NvOsMutexLock(hAboot->hStorageMutex);

    NV_CHECK_ERROR_CLEANUP(
        NvPartMgrGetIdByName(NvPartitionName, &PartitionId)
    );

    NV_CHECK_ERROR_CLEANUP(
        NvStorMgrFileQueryStat(NvPartitionName, &FileStat)
    );

    if (!FileStat.Size)
    {
        e = NvError_FileOperationFailed;
        goto fail;
    }

    pRead = NvOsAlloc(FileStat.Size);
    if (!pRead)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }

    NV_CHECK_ERROR_CLEANUP(
        NvStorMgrFileOpen(NvPartitionName, NVOS_OPEN_READ, &hFile)
    );

    NV_CHECK_ERROR_CLEANUP(NvStorMgrFileSeek(hFile, 0, NvOsSeek_Set));
    NV_CHECK_ERROR_CLEANUP(
        NvStorMgrFileRead(hFile, pRead, FileStat.Size, &BytesRead)
    );
    if (BytesRead != FileStat.Size)
    {
        e = NvError_EndOfFile;
        goto fail;
    }
    e = NvSuccess;

 fail:

    ec = NvStorMgrFileClose(hFile);
    NvOsMutexUnlock(hAboot->hStorageMutex);
    if ((e != NvSuccess) || (ec != NvSuccess))
    {
        NvOsFree(pRead);
        pRead = NULL;
        FileStat.Size = 0;
    }
    *pBuff = pRead;
    *pSize = FileStat.Size;

    if (e == NvSuccess)
        e = ec;

    return e;
}

/**
 * NvAbootBootfsWrite
 *   Writes Size bytes from pBuff into the partition named NvPartitionName
 *   in the NVFlash partition table.
 */

NvError NvAbootBootfsWrite(
    NvAbootHandle hAboot,
    const char   *NvPartitionName,
    const NvU8   *pBuff,
    NvU32         Size)
{
    NvStorMgrFileHandle hFile = NULL;
    NvU32               PartitionId;
    NvU32               BytesWritten;
    NvError             e, ec;

    if (!hAboot || !NvPartitionName || !pBuff || !Size)
        return NvError_BadParameter;

    NvOsMutexLock(hAboot->hStorageMutex);

    NV_CHECK_ERROR_CLEANUP(
        NvPartMgrGetIdByName(NvPartitionName, &PartitionId)
    );

    NV_CHECK_ERROR_CLEANUP(
        NvStorMgrFileOpen(NvPartitionName, NVOS_OPEN_WRITE, &hFile)
    );

    if (NvSysUtilCheckSparseImage((NvU8 *)pBuff, Size))
    {

        NV_CHECK_ERROR_CLEANUP(
          NvSysUtilUnSparse(hFile, (NvU8 *)pBuff, Size,
                            NV_TRUE,
                            NV_TRUE,
                            NV_FALSE,
                            NV_FALSE,
                            NULL,
                            NULL)
        );

    }
    else
    {
        NV_CHECK_ERROR_CLEANUP(
        NvStorMgrFileWrite(hFile, (void*)pBuff, Size, &BytesWritten)
        );

        if (BytesWritten != Size)
        {
            e = NvError_EndOfFile;
            goto fail;
        }
    }
    e = NvSuccess;

 fail:
    ec = NvStorMgrFileClose(hFile);
    NvOsMutexUnlock(hAboot->hStorageMutex);

    if (e == NvSuccess)
        e = ec;

    return e;
}

NvError NvAbootBootfsFormat(const char   *NvPartitionName)
{
    return NvStorMgrFormat(NvPartitionName);
}
