/**
 * Copyright (c) 2008-2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define NV_ENABLE_DEBUG_PRINTS 0

#include "nvos.h"
#include "nvassert.h"
#include "nvext2filesystem.h"
#include "nvpartmgr_defs.h"
#include "../basic/nvbasicfilesystem.h"

static NvOsMutexHandle s_NvExt2FSMutex;

typedef struct NvExt2FileSystemRec
{
    NvFileSystem hFileSystem;
    NvPartitionHandle hPartition;
    NvDdkBlockDevHandle hBlkDevHandle;
    char PartName[NVPARTMGR_MOUNTPATH_NAME_LENGTH];
    NvFileSystem* pBasicFileSystem;
}NvExt2FileSystem;


static NvError
NvFileSystemFileOpen(
    NvFileSystemHandle hFileSystem,
    const char *FilePath,
    NvU32 Flags,
    NvFileSystemFileHandle *phFile)
{
    NvExt2FileSystem *pNvExt2FileSystem = (NvExt2FileSystem*)hFileSystem;
    NvFileSystem* pBasicFS;
    NvError e = NvSuccess;
    if (!hFileSystem)
    {
        return NvSuccess;
    }
    pBasicFS = pNvExt2FileSystem->pBasicFileSystem;
    NvOsMutexLock(s_NvExt2FSMutex);
        NV_CHECK_ERROR_CLEANUP(
            pBasicFS->NvFileSystemFileOpen(pBasicFS,
                                        FilePath,
                                        Flags,
                                        phFile)
    );
fail:
    NvOsMutexUnlock(s_NvExt2FSMutex);
    return e;
}

static NvError
NvFileSystemFileQueryStat(
    NvFileSystemHandle hFileSystem,
    const char *FilePath,
    NvFileStat *pStat)
{
    NvExt2FileSystem *pNvExt2FileSystem = (NvExt2FileSystem*)hFileSystem;
    NvFileSystem* pBasicFS;
    NvError e = NvSuccess;
    if (!hFileSystem)
    {
        return NvSuccess;
    }
    pBasicFS = pNvExt2FileSystem->pBasicFileSystem;
    NvOsMutexLock(s_NvExt2FSMutex);
        NV_CHECK_ERROR_CLEANUP(
            pBasicFS->NvFileSystemFileQueryStat(pBasicFS,
                                        FilePath,
                                        pStat)
    );
fail:
    NvOsMutexUnlock(s_NvExt2FSMutex);
    return e;
}

NvError NvExt2FileSystemInit(void)
{
    NvError e = NvSuccess;
    if (!s_NvExt2FSMutex)
    {
        NV_CHECK_ERROR_CLEANUP(NvOsMutexCreate(&s_NvExt2FSMutex));
    }
    NV_CHECK_ERROR_CLEANUP(NvBasicFileSystemInit());
    return e;

fail:
    s_NvExt2FSMutex = NULL;
    return e;
}

void NvExt2FileSystemDeinit(void)
{
    NvOsMutexDestroy(s_NvExt2FSMutex);
    NvBasicFileSystemDeinit();
}

static NvError NvFileSystemUnmount(NvFileSystemHandle hFileSystem)
{
    NvExt2FileSystem *pNvExt2FileSystem = (NvExt2FileSystem*)hFileSystem;

    if (!hFileSystem)
    {
        return NvSuccess;
    }

    NvOsMutexLock(s_NvExt2FSMutex);
    pNvExt2FileSystem->pBasicFileSystem->NvFileSystemUnmount(pNvExt2FileSystem->pBasicFileSystem);
    NvOsFree(hFileSystem);
    NvOsMutexUnlock(s_NvExt2FSMutex);
    
    return NvSuccess;
}

static NvError NvFileSystemFormat(NvFileSystemHandle hFileSystem,
    NvU64 StartLogicalSector, NvU64 NumLogicalSectors,
    NvU32 PTendSector, NvBool IsPTpartition)
{
    NvError e = NvSuccess;
#if !NVOS_IS_WINDOWS
    NvExt2FileSystem *pNvExt2FileSystem = (NvExt2FileSystem*)hFileSystem;
    
    NvOsMutexLock(s_NvExt2FSMutex);

    NV_CHECK_ERROR_CLEANUP(
        NvExt2PrivFormatPartition(pNvExt2FileSystem->hBlkDevHandle,
           pNvExt2FileSystem->PartName)
    );

fail:
    NvOsMutexUnlock(s_NvExt2FSMutex);
#endif
    return e;
}


NvError
NvExt2FileSystemMount(
    NvPartitionHandle hPart,
    NvDdkBlockDevHandle hDevice,
    NvFsMountInfo *pMountInfo,
    NvU32 FileSystemAttr,
    NvFileSystemHandle *phFileSystem)
{
    NvError e = NvSuccess;
    NvExt2FileSystem *pNvExt2FileSystem = NULL;
        
    if (!hPart || !hDevice)
    {
        e = NvError_BadParameter;
        goto fail;
    }

    NvOsMutexLock(s_NvExt2FSMutex);
    pNvExt2FileSystem = NvOsAlloc(sizeof(NvExt2FileSystem));
    if (!pNvExt2FileSystem)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }


    NV_CHECK_ERROR_CLEANUP(
        NvBasicFileSystemMount(hPart, hDevice, pMountInfo,
                                                    FileSystemAttr, &pNvExt2FileSystem->pBasicFileSystem)
    );


    pNvExt2FileSystem->hFileSystem.NvFileSystemUnmount =
                            NvFileSystemUnmount;
    pNvExt2FileSystem->hFileSystem.NvFileSystemFileOpen =
                            NvFileSystemFileOpen;
    pNvExt2FileSystem->hFileSystem.NvFileSystemFileQueryStat =
                            NvFileSystemFileQueryStat;
    pNvExt2FileSystem->hFileSystem.NvFileSystemFileSystemQueryStat =
                            NULL;


    pNvExt2FileSystem->hFileSystem.NvFileSystemFormat = NvFileSystemFormat;

    pNvExt2FileSystem->hPartition = hPart;
    pNvExt2FileSystem->hBlkDevHandle = hDevice;
    NvOsStrncpy(pNvExt2FileSystem->PartName, pMountInfo->MountPath, 
        NVPARTMGR_MOUNTPATH_NAME_LENGTH);

    *phFileSystem = &pNvExt2FileSystem->hFileSystem;
    NvOsMutexUnlock(s_NvExt2FSMutex);
    return e;

fail:
    phFileSystem = NULL;
    NvOsMutexLock(s_NvExt2FSMutex);
    return e;
}


