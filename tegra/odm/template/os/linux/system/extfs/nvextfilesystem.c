/**
 * Copyright (c) 2008-2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvextfilesystem.h"
#include "nvfsmgr_defs.h"
#include "nvassert.h"

//Macros for error checkings
#define CHECK_PARAM(x) \
        if (!(x)) \
        { \
           NV_ASSERT(x); \
           return NvError_BadParameter; \
        }

#define CHECK_MEM(x) \
        if (!(x)) \
        { \
           e = NvError_InsufficientMemory; \
           goto fail; \
        }

typedef struct NvExternalFileSystemRec
{
    NvFileSystem hFileSystem;
    NvPartitionHandle hPartition;
    NvFsMountInfo *pMountInfo;
    NvU32 RefFileCount;
}NvExternalFileSystem;

typedef struct NvExternalFileSystemFileRec
{
    NvFileSystemFile hFileSystemFile;
    NvExternalFileSystem* pExtFs;
}NvExternalFileSystemFile;


NvError NvExternalFileSystemInit(void)
{
    return NvSuccess;
}

void NvExternalFileSystemDeinit(void)
{
}

static NvError NvFileSystemUnmount(NvFileSystemHandle hFileSystem)
{
    CHECK_PARAM(hFileSystem);
    NvOsFree(hFileSystem);
    hFileSystem = NULL;
    return NvSuccess;
}

static NvError
NvFileSystemFileQueryStat(
    NvFileSystemHandle hFileSystem,
    const char *FilePath,
    NvFileStat *pStat)
{
    return NvSuccess;
}

static NvError
NvFileSystemFileSystemQueryStat(
    NvFileSystemHandle hFileSystem,
    const char *FilePath,
    NvFileSystemStat *pStat)
{
    return NvError_NotSupported;
}

static NvError NvFileSystemFormat(NvFileSystemHandle hFileSystem,
    NvU64 StartLogicalSector, NvU64 NumLogicalSectors,
    NvU32 PTendSector, NvBool IsPTpartition)
{
    return NvSuccess;
}


static NvError
NvFileSystemFileClose(
    NvFileSystemFileHandle hFile)
{
    NvExternalFileSystemFile *pExtFsFile = NULL;
    pExtFsFile = (NvExternalFileSystemFile *)hFile;
    NvOsFree(pExtFsFile);
    return NvSuccess;
}

static NvError
NvFileSystemFileRead(
    NvFileSystemFileHandle hFile,
    void *pBuffer,
    NvU32 BytesToRead,
    NvU32 *BytesRead)
{
    *BytesRead = BytesToRead;
    return NvSuccess;
}

static NvError
NvFileSystemFileWrite(
    NvFileSystemFileHandle hFile,
    const void *pBuffer,
    NvU32 BytesToWrite,
    NvU32 *BytesWritten)
{
    *BytesWritten = BytesToWrite;
    return NvSuccess;
}

static NvError
NvFileSystemFileIoctl(
     NvFileSystemFileHandle hFile,
     NvFileSystemIoctlType IoctlType,
     NvU32 InputSize,
     NvU32 OutputSize,
     const void * InputArgs,
     void *OutputArgs)
{
    return NvSuccess;
}

static NvError
NvFileSystemFileSeek(
    NvFileSystemFileHandle hFile,
    NvS64 ByteOffset,
    NvU32 Whence)
{
    return NvSuccess;
}

static NvError
NvFileSystemFtell(
    NvFileSystemFileHandle hFile,
    NvU64 *ByteOffset)
{
    return NvSuccess;
}

static NvError
NvFileSystemFileQueryFstat(
    NvFileSystemFileHandle hFile,
    NvFileStat *pStat)
{
    return NvSuccess;
}

static NvError
NvFileSystemFileOpen(
    NvFileSystemHandle hFileSystem,
    const char *FilePath,
    NvU32 Flags,
    NvFileSystemFileHandle *phFile)
{
    
    NvError e = NvSuccess;
    NvExternalFileSystemFile *pExtFsFile = NULL;

    pExtFsFile = NvOsAlloc(sizeof(NvExternalFileSystemFile));
    CHECK_MEM(pExtFsFile);
    
    pExtFsFile->hFileSystemFile.NvFileSystemFileClose =
                                    NvFileSystemFileClose;
    pExtFsFile->hFileSystemFile.NvFileSystemFileRead =
                                    NvFileSystemFileRead;
    pExtFsFile->hFileSystemFile.NvFileSystemFileWrite =
                                    NvFileSystemFileWrite;
    pExtFsFile->hFileSystemFile.NvFileSystemFileIoctl =
                                    NvFileSystemFileIoctl;
    pExtFsFile->hFileSystemFile.NvFileSystemFileSeek =
                                    NvFileSystemFileSeek;
    pExtFsFile->hFileSystemFile.NvFileSystemFtell =
                                    NvFileSystemFtell;
    pExtFsFile->hFileSystemFile.NvFileSystemFileQueryFstat =
                                    NvFileSystemFileQueryFstat;
    *phFile = &pExtFsFile->hFileSystemFile;

fail:    
    return e;
}

NvError
NvExternalFileSystemMount(
    NvPartitionHandle hPart,
    NvDdkBlockDevHandle hDevice,
    NvFsMountInfo *pMountInfo,
    NvU32 FileSystemAttr,
    NvFileSystemHandle *phFileSystem)
{
    NvExternalFileSystem *pExtFs;
    NvError e = NvSuccess;

    CHECK_PARAM(pMountInfo);

    pExtFs = NvOsAlloc(sizeof(NvExternalFileSystem));
    CHECK_MEM(pExtFs);

    pExtFs->hPartition = hPart;
    pExtFs->pMountInfo = pMountInfo;

    pExtFs->hFileSystem.NvFileSystemUnmount =
                            NvFileSystemUnmount;
    pExtFs->hFileSystem.NvFileSystemFileOpen =
                            NvFileSystemFileOpen;
    pExtFs->hFileSystem.NvFileSystemFileQueryStat =
                            NvFileSystemFileQueryStat;
    pExtFs->hFileSystem.NvFileSystemFileSystemQueryStat =
                            NvFileSystemFileSystemQueryStat;
    pExtFs->hFileSystem.NvFileSystemFormat = NvFileSystemFormat;

    pExtFs->RefFileCount = 0; //Init the RefFile Count to 0
    *phFileSystem = &pExtFs->hFileSystem;

fail:
    return e;
}

