/**
 * Copyright (c) 2008-2013, NVIDIA CORPORATION. All rights reserved.
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
#include "nvbasicfilesystem.h"
#include "nvpartmgr_defs.h"

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

typedef struct NvBasicFileSystemRec
{
    NvFileSystem hFileSystem;
    NvPartitionHandle hPartion;
    NvDdkBlockDevHandle hBlkDevHandle;
    NvU32 RefFileCount;
}NvBasicFileSystem;

typedef struct NvBasicFileSystemFileRec
{
    NvFileSystemFile hFileSystemFile;
    NvU64 MaxFileSize;
    NvU64 FileOffset;
    NvU32 SectorNum;
    NvBasicFileSystem* pBasicFileSystem;
    NvU8* pFileCachedBuffer;
    char* pFileName;
    NvU32 OpenMode;
    NvU32 CachedBufferSize;
    NvU32 BytesPerSector;
}NvBasicFileSystemFile;

typedef struct NvBasicFSFileNameListRec
{
    char *FilePath;
    struct NvBasicFSFileNameListRec* pNext;
}NvBasicFSFileNameList;

//This is the head of the single linked list of all the open files
static NvBasicFSFileNameList* s_NvBasicFSFileNameList = NULL;

//Mutex to provide exclusion while accessing file system functions
static NvOsMutexHandle s_NvBasicFSFileMutex;
static NvOsMutexHandle s_NvBasicFSMutex;


/**
 * AddToNvBasicFSList()
 *
 * Adds FileName to the FileName list
 *
 * @ param pSearchPathString pointer to file name to be addded
 *
 * @ retval NvSuccess if success
 * @ retval NvError_InvalidParameter if invalid name
 * @ retval NvError_InsufficientMemory if memory alocation fails
 */

static NvError AddToNvBasicFSList(const char *pSearchPathString)
{
    NvError e = NvSuccess;
    NvBasicFSFileNameList* pNode = NULL;
    NvU32 SizeOfString = 0;

    CHECK_PARAM(pSearchPathString)
    pNode = NvOsAlloc(sizeof(NvBasicFSFileNameList));
    CHECK_MEM(pNode);

    SizeOfString = NvOsStrlen(pSearchPathString);
    pNode->FilePath = NULL;
    pNode->pNext = NULL;
    pNode->FilePath = NvOsAlloc(SizeOfString + 1);
    CHECK_MEM(pNode->FilePath);
    NvOsStrncpy(pNode->FilePath, pSearchPathString, SizeOfString + 1);
    pNode->FilePath[SizeOfString] = '\0';

    if (s_NvBasicFSFileNameList == NULL)
        s_NvBasicFSFileNameList = pNode;
    else
    {
        pNode->pNext = s_NvBasicFSFileNameList;
        s_NvBasicFSFileNameList = pNode;
    }
    return e;

fail:
    if (pNode)
    {
        if (pNode->FilePath)
            NvOsFree(pNode->FilePath);
        NvOsFree(pNode);
    }
    return e;
}

/**
 * RemoveFromNvBasicFSList()
 *
 * removes the node from the cache FS list which contains the sent file name
 *
 * @param pSearchPathString filename to be removed
 *
 * @retval NvSuccess if success
 * @retval NvError_InvalidParameter if invalid filename or File list is NULL
 */

static NvError RemoveFromNvBasicFSList(const char *pSearchPathString)
{
    NvError Err = NvSuccess;
    NvBasicFSFileNameList* pCurNode = NULL;
    NvBasicFSFileNameList* pPrevNode = NULL;
    CHECK_PARAM(s_NvBasicFSFileNameList);
    CHECK_PARAM(pSearchPathString);

    pCurNode = s_NvBasicFSFileNameList;
    // Check whether the current node has same file name or not.
    // If yes, then break the loop
    while ((pCurNode != NULL) &&
        (NvOsStrcmp(pCurNode->FilePath, pSearchPathString)))
    {
        // Requested file is not found yet
        pPrevNode = pCurNode;
        pCurNode = pCurNode->pNext;
    }
    if (pCurNode == NULL)
        Err = NvError_BadParameter;
    else
    {
        if (pPrevNode)
        {
            pPrevNode->pNext = pCurNode->pNext;
        }
        else
        {
            // if pCurNote is the only element, then its pCurNode->pNext
            // is always NULL. Or else it contains next node address
            s_NvBasicFSFileNameList = pCurNode->pNext;
        }
        NvOsFree(pCurNode->FilePath);
        NvOsFree(pCurNode);
    }
    return Err;
}

/**
 * InNvBasicFSList()
 *
 * Checks if a given path exist in File name list or not
 *
 * @param pSearchPathString string to be checked in FS list
 *
 * @retval NV_TRUE if name exist in FS list
 * @retval NV_FALSE if name does not exist in FS list
 */

static NvBool InNvBasicFSList(const char *pSearchPathString)
{
    NvBasicFSFileNameList* pTempFileList = NULL;
    NV_ASSERT(pSearchPathString);
    if ((pSearchPathString == NULL) || (s_NvBasicFSFileNameList == NULL))
       return NV_FALSE;
    else
    {
        pTempFileList = s_NvBasicFSFileNameList;
        // Check whether the current node has same file name or not.
        // If yes, then break the loop
        while ((pTempFileList != NULL) &&
            (NvOsStrcmp(pTempFileList->FilePath, pSearchPathString)))
                pTempFileList = pTempFileList->pNext;
    }
    return (pTempFileList == NULL)? NV_FALSE:NV_TRUE;
}

NvError NvBasicFileSystemInit(void)
{
    NvError e = NvSuccess;
    if (!s_NvBasicFSFileMutex)
        NV_CHECK_ERROR_CLEANUP(NvOsMutexCreate(&s_NvBasicFSFileMutex));
    if (!s_NvBasicFSMutex)
        NV_CHECK_ERROR_CLEANUP(NvOsMutexCreate(&s_NvBasicFSMutex));
    return e;

fail:
    if (s_NvBasicFSFileMutex)
        NvOsMutexDestroy(s_NvBasicFSFileMutex);
    s_NvBasicFSFileMutex = NULL;
    s_NvBasicFSMutex = NULL;
    return e;
}

void NvBasicFileSystemDeinit(void)
{
    if (s_NvBasicFSFileMutex)
    {
        NvOsMutexDestroy(s_NvBasicFSFileMutex);
        s_NvBasicFSFileMutex = NULL;
    }

    if (s_NvBasicFSMutex)
    {
        NvOsMutexDestroy(s_NvBasicFSMutex);
        s_NvBasicFSMutex = NULL;
    }
}

/*
    This will unmount Basic file system. It frees memory for Basic file system
    only when the open file count is zero. It returns AccessDeinied if open
    reference count is not zero. Client has to call FileClose before Unmount
*/
static NvError NvFileSystemUnmount(NvFileSystemHandle hFileSystem)
{
    NvError Err = NvSuccess;
    NvBasicFileSystem *pNvBasicFileSystem = NULL;
    CHECK_PARAM(hFileSystem);
    NvOsMutexLock(s_NvBasicFSMutex);
    pNvBasicFileSystem = (NvBasicFileSystem *)hFileSystem;
    if (pNvBasicFileSystem->RefFileCount !=0)
    {
        Err = NvError_AccessDenied;
        NV_DEBUG_PRINTF(("NvFileSystemUnmount: Error One or more. "
                    "Files still open but Unmount issued. \n"));
    }
    else
    {
        NvOsFree(hFileSystem);
        hFileSystem = NULL;
    }
    NvOsMutexUnlock(s_NvBasicFSMutex);
    return Err;
}

static NvError
NvFileSystemFileQueryStat(
    NvFileSystemHandle hFileSystem,
    const char *FilePath,
    NvFileStat *pStat)
{
    NvError Err = NvSuccess;
    NvDdkBlockDevInfo BlockDevInfo;
    NvBasicFileSystem *pNvBasicFileSystem = NULL;
    NvDdkBlockDevHandle hBlkDev = NULL;

    CHECK_PARAM(hFileSystem);
    CHECK_PARAM(pStat);
    pNvBasicFileSystem = (NvBasicFileSystem *)hFileSystem;
    hBlkDev = pNvBasicFileSystem->hBlkDevHandle;
    hBlkDev->NvDdkBlockDevGetDeviceInfo(hBlkDev, &BlockDevInfo);
    if (!BlockDevInfo.TotalBlocks)
    {
        Err = NvError_BadParameter;
        NV_DEBUG_PRINTF(("NvFileSystemFileQueryStat Failed. Either device"
                " is not present or its not responding. \n"));
        goto Fail;
    }
    // File size is not supported in Basic File system so
    // give the patition size in bytes as file size
    pStat->Size = ((BlockDevInfo.BytesPerSector) *
            (pNvBasicFileSystem->hPartion->NumLogicalSectors));
    return NvSuccess;
Fail:
    pStat->Size = 0;
    return Err;
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
    NvDdkBlockDevHandle hBlkDev = NULL;
    NvBasicFileSystem *pNvBasicFileSystem = NULL;
    NvError Err = NvSuccess;
    NvDdkBlockDevIoctl_EraseLogicalSectorsInputArgs ErasePartArgs;

    CHECK_PARAM(hFileSystem);
    NvOsMutexLock(s_NvBasicFSFileMutex);

    pNvBasicFileSystem = (NvBasicFileSystem *)hFileSystem;
    // Call block driver partition Erase API
    hBlkDev = pNvBasicFileSystem->hBlkDevHandle;
    ErasePartArgs.StartLogicalSector = (NvU32)StartLogicalSector;
    ErasePartArgs.NumberOfLogicalSectors = (NvU32)NumLogicalSectors;
    ErasePartArgs.PTendSector = PTendSector;
    ErasePartArgs.IsPTpartition = IsPTpartition;
    ErasePartArgs.IsTrimErase = NV_FALSE;
    ErasePartArgs.IsSecureErase = NV_FALSE;
    Err = hBlkDev->NvDdkBlockDevIoctl(hBlkDev, 
            NvDdkBlockDevIoctlType_ErasePartition, 
            sizeof(NvDdkBlockDevIoctl_EraseLogicalSectorsInputArgs), 0,
            &ErasePartArgs, NULL);

    NvOsMutexUnlock(s_NvBasicFSFileMutex);
    return Err;
}


static NvError
NvFileSystemFileClose(
    NvFileSystemFileHandle hFile)
{
    NvError e = NvSuccess;
    NvError Err = NvSuccess;
    NvBasicFileSystemFile *pNvBasicFileSystemFile = NULL;
    NvBasicFileSystem *pNvBasicFileSystem = NULL;
    NvU8* pTempBuff = NULL;
    NvDdkBlockDevHandle hBlkDev = NULL;
    CHECK_PARAM(hFile);
    NvOsMutexLock(s_NvBasicFSFileMutex);
    pNvBasicFileSystemFile = (NvBasicFileSystemFile *)hFile;
    pNvBasicFileSystem = pNvBasicFileSystemFile->pBasicFileSystem;
    pTempBuff = pNvBasicFileSystemFile->pFileCachedBuffer;


    // Check whether file is opened in write mode or not
    if (pNvBasicFileSystemFile->OpenMode == NVOS_OPEN_WRITE)
    {
        // Flush data to media if any thing left in file cache buffer
        if (pNvBasicFileSystemFile->CachedBufferSize)
        {

            hBlkDev = pNvBasicFileSystem->hBlkDevHandle;
            Err = hBlkDev->NvDdkBlockDevWriteSector(hBlkDev,
                        pNvBasicFileSystemFile->SectorNum,
                        pTempBuff,
                        1);
        }
    }

    e = RemoveFromNvBasicFSList(pNvBasicFileSystemFile->pFileName);

    if (pNvBasicFileSystem->RefFileCount)
    {
        pNvBasicFileSystem->RefFileCount--;
    }

    if (pTempBuff)
    {
        NvOsFree(pTempBuff);
        pNvBasicFileSystemFile->pFileCachedBuffer = NULL;
    }

    if (pNvBasicFileSystemFile->pFileName)
    {
        NvOsFree(pNvBasicFileSystemFile->pFileName);
        pNvBasicFileSystemFile->pFileName = NULL;
    }

    NvOsFree(pNvBasicFileSystemFile);
    hFile = NULL;

    if (Err != NvSuccess)
    {
        e = Err;
    }

    NvOsMutexUnlock(s_NvBasicFSFileMutex);
    return e;
}

static NvError
NvFileSystemFileRead(
    NvFileSystemFileHandle hFile,
    void *pBuffer,
    NvU32 BytesToRead,
    NvU32 *BytesRead)
{
    NvError Err = NvSuccess;
    NvU32 NumOfPagesToRead = 0;
    NvU32 BytesToCopy = 0;
    NvU32 MaxCachedBuffSize = 0;
    NvU32 ResidualLength = 0;
    NvBasicFileSystemFile *pNvBasicFileSystemFile = NULL;
    NvDdkBlockDevHandle hBlockDeviceDriver = NULL;
    NvU8* pClientBuffer = (NvU8 *)pBuffer;
    NvU8* pFileBuffer = NULL;

    CHECK_PARAM(hFile);
    CHECK_PARAM(pBuffer);
    CHECK_PARAM(BytesRead);
    pNvBasicFileSystemFile = (NvBasicFileSystemFile*)hFile;
    CHECK_PARAM(pNvBasicFileSystemFile->OpenMode ==
        NVOS_OPEN_READ);
    hBlockDeviceDriver =
        pNvBasicFileSystemFile->pBasicFileSystem->hBlkDevHandle;
    MaxCachedBuffSize = pNvBasicFileSystemFile->BytesPerSector;
    pFileBuffer = pNvBasicFileSystemFile->pFileCachedBuffer;
    *BytesRead = 0;
    // Verify File Limit
    if ((pNvBasicFileSystemFile->FileOffset + BytesToRead) >
        pNvBasicFileSystemFile->MaxFileSize)
    {
        Err = NvError_InvalidSize;
        NV_DEBUG_PRINTF(("BASIC FS Read: Number of bytes to read exceeds "
                                "max file size limit. File Read failed. \n"));
        goto ReturnStatus;
    }
    while (BytesToRead)
    {
        if (pNvBasicFileSystemFile->CachedBufferSize)
        {
            BytesToCopy = (BytesToRead <=
                    pNvBasicFileSystemFile->CachedBufferSize) ?
                    BytesToRead : pNvBasicFileSystemFile->CachedBufferSize;

            // Copy cached buffer into client buffer
            NvOsMemcpy(pClientBuffer, pFileBuffer + (MaxCachedBuffSize -
                    pNvBasicFileSystemFile->CachedBufferSize), BytesToCopy);

            BytesToRead -= BytesToCopy;
            pClientBuffer += BytesToCopy;
            *BytesRead += BytesToCopy;
            pNvBasicFileSystemFile->FileOffset += BytesToCopy;
            pNvBasicFileSystemFile->CachedBufferSize -= BytesToCopy;
        }
        else
        {
            // There is no data avaialable in cache. Need to read from Device
            // Calculate number of pages to read from device
            NumOfPagesToRead = (BytesToRead / MaxCachedBuffSize);
            ResidualLength = (BytesToRead % MaxCachedBuffSize);
            if (NumOfPagesToRead)
            {
                Err = hBlockDeviceDriver->NvDdkBlockDevReadSector(
                        hBlockDeviceDriver,
                        pNvBasicFileSystemFile->SectorNum,
                        pClientBuffer,
                        NumOfPagesToRead);

                if (Err != NvSuccess)
                {
                    Err = NvError_FileReadFailed;
                    NV_DEBUG_PRINTF(("BASIC FS Read: Device driver Read "
                                            "Failed. File Read Failed \n"));
                    goto ReturnStatus;
                }
                BytesToRead -= NumOfPagesToRead * MaxCachedBuffSize;
                pClientBuffer += NumOfPagesToRead * MaxCachedBuffSize;
                *BytesRead += NumOfPagesToRead * MaxCachedBuffSize;
                pNvBasicFileSystemFile->FileOffset += NumOfPagesToRead *
                                                            MaxCachedBuffSize;

                pNvBasicFileSystemFile->SectorNum += NumOfPagesToRead;
            }
            if (ResidualLength)
            {
                // Read one sector to fill remaining bytes
                Err = hBlockDeviceDriver->NvDdkBlockDevReadSector(
                            hBlockDeviceDriver,
                            pNvBasicFileSystemFile->SectorNum,
                            pFileBuffer,
                            1);

                if (Err != NvSuccess)
                {
                    NV_DEBUG_PRINTF(("BasicFS Read: Device driver Read Failed "
                                        "File Read Failed \n"));
                    Err = NvError_FileReadFailed;
                    goto ReturnStatus;
                }
                NvOsMemcpy(pClientBuffer, pFileBuffer, ResidualLength);
                BytesToRead -= ResidualLength;
                *BytesRead += ResidualLength;
                pNvBasicFileSystemFile->SectorNum++;
                pNvBasicFileSystemFile->FileOffset += ResidualLength;
                pNvBasicFileSystemFile->CachedBufferSize = MaxCachedBuffSize -
                                                        ResidualLength;

            }
        }
    }

ReturnStatus:
    return Err;
}

static NvError
NvFileSystemFileWrite(
    NvFileSystemFileHandle hFile,
    const void *pBuffer,
    NvU32 BytesToWrite,
    NvU32 *BytesWritten)
{
    NvError Err = NvSuccess;
    NvU32 NumOfPagesToWrite = 0;
    NvU32 BytesToCopy = 0;
    NvU32 ResidualBytes = 0;
    NvU32 MaxCachedBufSize = 0;
    NvDdkBlockDevHandle hBlockDeviceDriver = NULL;
    NvU8* pClientBuffer = (NvU8 *)pBuffer;
    NvU8* pFileBuffer = NULL;
    NvBasicFileSystemFile *pNvBasicFileSystemFile = NULL;

    CHECK_PARAM(hFile);
    CHECK_PARAM(pBuffer);
    CHECK_PARAM(BytesWritten);
    pNvBasicFileSystemFile = (NvBasicFileSystemFile*)hFile;
    CHECK_PARAM(pNvBasicFileSystemFile->OpenMode ==
        NVOS_OPEN_WRITE);
    hBlockDeviceDriver =
        pNvBasicFileSystemFile->pBasicFileSystem->hBlkDevHandle;
    MaxCachedBufSize = pNvBasicFileSystemFile->BytesPerSector;
    pFileBuffer = pNvBasicFileSystemFile->pFileCachedBuffer;
    *BytesWritten = 0;
    // Verify File limit
    if ((pNvBasicFileSystemFile->FileOffset + BytesToWrite) >
            pNvBasicFileSystemFile->MaxFileSize)
    {
        Err = NvError_InvalidSize;
        NV_DEBUG_PRINTF(("BasicFS Write: Number of bytes to write exceeds "
                                "max file size limit. File Write Failed \n"));
        goto StatuReturn;
    }

    while (BytesToWrite)
    {
        // Required to avoid alternate write operation of MaxCachedBufSize and
        // (BytesToWrite-MaxCachedBufSize)if BytesToWrite is multiple of
        // MaxCachedBufSize
        if (pNvBasicFileSystemFile->CachedBufferSize)
        {
             BytesToCopy = (BytesToWrite < (MaxCachedBufSize -
                     pNvBasicFileSystemFile->CachedBufferSize)) ? BytesToWrite :
                     (MaxCachedBufSize - pNvBasicFileSystemFile->CachedBufferSize);

             if (BytesToCopy)
             {
                 NvOsMemcpy(pFileBuffer + pNvBasicFileSystemFile->CachedBufferSize,
                     pClientBuffer,BytesToCopy);

                 BytesToWrite -= BytesToCopy;
                 pNvBasicFileSystemFile->CachedBufferSize += BytesToCopy;
                 *BytesWritten += BytesToCopy;
                 pNvBasicFileSystemFile->FileOffset += BytesToCopy;
                 pClientBuffer += BytesToCopy;
                 // Data in File cache buffer is written into media only when it is
                 // full and will be written in next while loop. Or it will be
                 // written when File Close is called
                 continue;
             }
             else
             {
                 // If reached here means file cache buffer is full. So, write it
                 // into device
                 Err = hBlockDeviceDriver->NvDdkBlockDevWriteSector(
                         hBlockDeviceDriver,
                         pNvBasicFileSystemFile->SectorNum++,
                         pFileBuffer,
                         1);
                 if (Err != NvSuccess)
                 {
                     NV_DEBUG_PRINTF(("BasicFS Write: Device driver Write "
                                                "Failed. File Write failed. \n"));
                     Err = NvError_FileWriteFailed;
                     goto StatuReturn;
                 }
                 pNvBasicFileSystemFile->CachedBufferSize = 0;
             }
        }
        NumOfPagesToWrite = BytesToWrite / MaxCachedBufSize;
        ResidualBytes = BytesToWrite % MaxCachedBufSize;
        if (NumOfPagesToWrite)
        {
            Err = hBlockDeviceDriver->NvDdkBlockDevWriteSector(
                    hBlockDeviceDriver,
                    pNvBasicFileSystemFile->SectorNum,
                    pClientBuffer,
                    NumOfPagesToWrite);

            if (Err != NvSuccess)
            {
                NV_DEBUG_PRINTF(("BasicFS Write: Device driver Write "
                                            "Failed. File Write failed. \n"));

                Err = NvError_FileWriteFailed;
                goto StatuReturn;
            }
            BytesToWrite -= NumOfPagesToWrite * MaxCachedBufSize;
            pNvBasicFileSystemFile->SectorNum += NumOfPagesToWrite;
            *BytesWritten += NumOfPagesToWrite * MaxCachedBufSize;
            pNvBasicFileSystemFile->FileOffset += NumOfPagesToWrite *
                        MaxCachedBufSize;

            pClientBuffer += NumOfPagesToWrite * MaxCachedBufSize;
        }
        if (ResidualBytes)
        {
            // ResidualBytes is in the next sector. So, clear pFileBuffer and
            // read back the sector first.
            NvOsMemset(pFileBuffer, 0, MaxCachedBufSize);
            Err = hBlockDeviceDriver->NvDdkBlockDevReadSector(
                    hBlockDeviceDriver,
                    pNvBasicFileSystemFile->SectorNum,
                    pFileBuffer,
                    1);
            NvOsMemcpy(pFileBuffer, pClientBuffer, ResidualBytes);
            BytesToWrite -= ResidualBytes;
            *BytesWritten += ResidualBytes;
            pNvBasicFileSystemFile->FileOffset += ResidualBytes;
            pClientBuffer +=ResidualBytes;
            pNvBasicFileSystemFile->CachedBufferSize = ResidualBytes;
        }
    }

StatuReturn:
    return Err;
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
    NvError Err = NvSuccess;
    NvBasicFileSystemFile *pNvBasicFileSystemFile = NULL;
    CHECK_PARAM(hFile);
    pNvBasicFileSystemFile = (NvBasicFileSystemFile *)hFile;

    switch (IoctlType)
    {
        case NvFileSystemIoctlType_SetCryptoHashAlgo:
        case NvFileSystemIoctlType_QueryIsEncrypted:
        case NvFileSystemIoctlType_QueryIsHashed:
        case NvFileSystemIoctlType_GetStoredHash:
        case NvFileSystemIoctlType_GetCurrentHash:
        case NvFileSystemIoctlType_IsValidHash:
            // Hashing and Cipher is not supported in Basic File system
            Err = NvError_NotSupported;
            break;

        case NvFileSystemIoctlType_WriteVerifyModeSelect:
        {
            NvFileSystemIoctl_WriteVerifyModeSelectInputArgs *WriteVerifyMode =
                (NvFileSystemIoctl_WriteVerifyModeSelectInputArgs *)InputArgs;

            NvDdkBlockDevIoctl_WriteVerifyModeSelectInputArgs
                BlkDrvrWriteVerifyIoctlArgs;

            NvDdkBlockDevHandle hBlockDeviceDriver =
                pNvBasicFileSystemFile->pBasicFileSystem->hBlkDevHandle;

            CHECK_PARAM(InputArgs);
            CHECK_PARAM(InputSize ==
                sizeof(NvFileSystemIoctl_WriteVerifyModeSelectInputArgs));

            BlkDrvrWriteVerifyIoctlArgs.IsReadVerifyWriteEnabled =
                WriteVerifyMode->ReadVerifyWriteEnabled;

            Err = hBlockDeviceDriver->NvDdkBlockDevIoctl(
                        hBlockDeviceDriver,
                        NvDdkBlockDevIoctlType_WriteVerifyModeSelect,
                        sizeof(NvDdkBlockDevIoctl_WriteVerifyModeSelectInputArgs),
                        0,
                        &BlkDrvrWriteVerifyIoctlArgs,
                        NULL);
            break;
        }
        case NvFileSystemIoctlType_WriteTagDataDisable:
            break;
        default:
            Err = NvError_BadParameter;
    }
    return Err;
}

static NvError
NvFileSystemFileSeek(
    NvFileSystemFileHandle hFile,
    NvS64 ByteOffset,
    NvU32 Whence)
{
    NvError Err = NvSuccess;
    NvU32 MaxCachedBuffSize = 0;
    NvU32 SectorNumber = 0;
    NvU32 FileStartSectorNum = 0;
    NvS64 NewFilePosition = 0;
    NvU32 OffsetInCache = 0;
    NvBasicFileSystemFile *pNvBasicFileSystemFile = NULL;
    NvPartitionHandle TmpPartitionHandle = NULL;
    NvDdkBlockDevHandle hBlockDeviceDriver = NULL;
    NvU8* pFileBuffer = NULL;

    CHECK_PARAM(hFile);
    pNvBasicFileSystemFile = (NvBasicFileSystemFile*)hFile;

    hBlockDeviceDriver =
        pNvBasicFileSystemFile->pBasicFileSystem->hBlkDevHandle;
    TmpPartitionHandle = pNvBasicFileSystemFile->pBasicFileSystem->hPartion;
    MaxCachedBuffSize = pNvBasicFileSystemFile->BytesPerSector;
    pFileBuffer = pNvBasicFileSystemFile->pFileCachedBuffer;
    FileStartSectorNum = (NvU32)TmpPartitionHandle->StartLogicalSectorAddress;
    if (pNvBasicFileSystemFile->OpenMode == NVOS_OPEN_READ)
    {

        // compute new file position depending on the seek start position
        switch (Whence)
        {
            case NvOsSeek_Set:
                NewFilePosition = ByteOffset;
                break;
            case NvOsSeek_Cur:
                NewFilePosition =
                    (pNvBasicFileSystemFile->FileOffset + ByteOffset);
                break;
            case NvOsSeek_End:
            default:
                NV_DEBUG_PRINTF(("BASIC FS Seek: Invalid File Seek start"
                                                " position. \n"));
                return NvError_BadParameter;
        }

        // Verify File Limit
        if ((NewFilePosition < 0) ||
            (NewFilePosition > (NvS64)pNvBasicFileSystemFile->MaxFileSize))
        {
            Err = NvError_InvalidSize;
            NV_DEBUG_PRINTF(("BASIC FS Seek: Invalid seek position \n"));
            goto ReturnStatus;
        }

        // Calculate page number and offset
        SectorNumber = ((NvU32)(NewFilePosition / MaxCachedBuffSize) +
            FileStartSectorNum);

        OffsetInCache = (NvU32)(NewFilePosition % MaxCachedBuffSize);

        // Read sector sector if cache does not already contains the existing sector.
        // And update sector number
        if (((SectorNumber + 1) != pNvBasicFileSystemFile->SectorNum) &&
            OffsetInCache)
        {
            // Read page number
            pNvBasicFileSystemFile->SectorNum = SectorNumber;
            Err = hBlockDeviceDriver->NvDdkBlockDevReadSector(
                    hBlockDeviceDriver,
                    pNvBasicFileSystemFile->SectorNum++,
                    pFileBuffer,
                    1);

            if (Err != NvSuccess)
            {
                NV_DEBUG_PRINTF(("BasicFS Seek: Device driver Read Failed "
                                    "File Seek Failed \n"));
                goto ReturnStatus;
            }
            pNvBasicFileSystemFile->CachedBufferSize =
                MaxCachedBuffSize - OffsetInCache;
        }
        else if (OffsetInCache)
        {
            pNvBasicFileSystemFile->SectorNum = SectorNumber + 1;
        }
        else
        {
            pNvBasicFileSystemFile->SectorNum = SectorNumber;
        }

        // Update Cached Buffered Size value
        if (OffsetInCache)
        {
                pNvBasicFileSystemFile->CachedBufferSize =
                    MaxCachedBuffSize - OffsetInCache;
        }
        else
        {
            pNvBasicFileSystemFile->CachedBufferSize = 0;
        }
    }
    else if (pNvBasicFileSystemFile->OpenMode == NVOS_OPEN_WRITE)
    {
        switch (Whence)
        {
            case NvOsSeek_Set:
                 if (ByteOffset < (NvS64)pNvBasicFileSystemFile->FileOffset)
                {
                    // In write mode, backward seek is not allowed
                    Err = NvError_InvalidSize;
                    NV_DEBUG_PRINTF(("BASIC FS Seek: Invalid seek "
                        "position \n"));

                    goto ReturnStatus;
                }
                NewFilePosition = ByteOffset;
                break;
            case NvOsSeek_Cur:
                if (ByteOffset < 0)
                {
                    // In write mode, backward seek is not allowed
                    Err = NvError_InvalidSize;
                    NV_DEBUG_PRINTF(("BASIC FS Seek: Invalid seek "
                        "position \n"));
                    goto ReturnStatus;
                }
                NewFilePosition =
                    (pNvBasicFileSystemFile->FileOffset + ByteOffset);

                break;
            case NvOsSeek_End:
            default:
                NV_DEBUG_PRINTF(("BASIC FS Seek: Invalid File Seek start"
                                                " position. \n"));
                return NvError_BadParameter;
        }

        // Verify File Limit
        if ((NewFilePosition < 0) ||
            (NewFilePosition > (NvS64)pNvBasicFileSystemFile->MaxFileSize))
        {
            Err = NvError_InvalidSize;
            NV_DEBUG_PRINTF(("BASIC FS Seek: Invalid seek position \n"));
            goto ReturnStatus;
        }

        // Calculate page number and offset
        SectorNumber = ((NvU32)(NewFilePosition / MaxCachedBuffSize) +
            FileStartSectorNum);

        OffsetInCache = (NvU32)(NewFilePosition % MaxCachedBuffSize);

        // Check if the required sector number is already in the cached buffer
        if (SectorNumber == pNvBasicFileSystemFile->SectorNum)
        {
            // Buffer is already in cache.
            // Now check whether file offset is same as the new file position.
            // If it is not then memset with 0 upto the new position.
            if (NewFilePosition != (NvS64)pNvBasicFileSystemFile->FileOffset)
            {
                // If New file position is not same as File Offset then new
                // position is always beyond current File Offset

                pNvBasicFileSystemFile->CachedBufferSize +=
                    (NvU32)(NewFilePosition -
                        pNvBasicFileSystemFile->FileOffset);

            }
        }
        else
        {
            // New file position is in the next page. So, flush 
            // cached buffer first
            if (pNvBasicFileSystemFile->CachedBufferSize)
            {
                Err = hBlockDeviceDriver->NvDdkBlockDevWriteSector(
                        hBlockDeviceDriver,
                        pNvBasicFileSystemFile->SectorNum,
                        pFileBuffer,
                        1);

                if (Err != NvSuccess)
                {
                    NV_DEBUG_PRINTF(("BasicFS Write: Device driver Write "
                            "Failed. File Seek failed. \n"));

                    Err = NvError_FileWriteFailed;
                    goto ReturnStatus;
                }
                pNvBasicFileSystemFile->CachedBufferSize = 0;
            }

            // Check whether new file position is at page boundary
            if (OffsetInCache)
            {
                // New file position is not at page boundary.
                // Memset the cache buffer with 0.
                NvOsMemset(pFileBuffer, 0, MaxCachedBuffSize);
                Err = hBlockDeviceDriver->NvDdkBlockDevReadSector(
                        hBlockDeviceDriver,
                        SectorNumber,
                        pFileBuffer,
                        1);
                if (Err != NvSuccess)
                {
                    NV_DEBUG_PRINTF(("BasicFS Read: Device driver Read "
                            "Failed. File Seek failed. \n"));

                    Err = NvError_FileReadFailed;
                    goto ReturnStatus;
                }
            }
            pNvBasicFileSystemFile->SectorNum = SectorNumber;
            pNvBasicFileSystemFile->CachedBufferSize = OffsetInCache;
        }
    }
    else
    {
        // Invalid file mode
        Err = NvError_NotSupported;
        goto ReturnStatus;
    }

    // Update file offset
    pNvBasicFileSystemFile->FileOffset = NewFilePosition;

ReturnStatus:
    return Err;
}

static NvError
NvFileSystemFtell(
    NvFileSystemFileHandle hFile,
    NvU64 *ByteOffset)
{
    NvBasicFileSystemFile *pNvBasicFileSystemFile = NULL;
    CHECK_PARAM(hFile);
    CHECK_PARAM(ByteOffset);
    pNvBasicFileSystemFile = (NvBasicFileSystemFile*)hFile;
    *ByteOffset = pNvBasicFileSystemFile->FileOffset;
    return NvSuccess;
}

static NvError
NvFileSystemFileQueryFstat(
    NvFileSystemFileHandle hFile,
    NvFileStat *pStat)
{
    NvBasicFileSystemFile *pNvBasicFileSystemFile = NULL;
    CHECK_PARAM(hFile);
    CHECK_PARAM(pStat);
    pNvBasicFileSystemFile = (NvBasicFileSystemFile *)hFile;
    // File size is not supported in Basic File system so
    // give the patition size in bytes as file size
    pStat->Size = pNvBasicFileSystemFile->MaxFileSize;
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
    NvBasicFileSystem *pNvBasicFileSystem = NULL;
    NvBasicFileSystemFile *pNvBasicFileSystemFile = NULL;
    NvDdkBlockDevInfo* pBlockDevInfo = NULL;
    NvDdkBlockDevHandle hBlkDev = NULL;
    NvU32 FileNameLength = 0;
    NvBool FileOpened = NV_FALSE;

    CHECK_PARAM(hFileSystem);
    CHECK_PARAM(FilePath);
    CHECK_PARAM(phFile);
    NvOsMutexLock(s_NvBasicFSFileMutex);
    // Check if file is already opened
    FileOpened = InNvBasicFSList(FilePath);
    if (FileOpened == NV_TRUE)
    {
        e = NvError_AccessDenied;
        goto fail;
    }
    pNvBasicFileSystem = (NvBasicFileSystem*)hFileSystem;
    // Check whether there are already files opened on this handle
    if (pNvBasicFileSystem->RefFileCount)
    {
        // Do not goto fail in this case.
        // Return error from here
        NvOsMutexUnlock(s_NvBasicFSFileMutex);
        return NvError_AccessDenied;
    }
    pBlockDevInfo = NvOsAlloc(sizeof(NvDdkBlockDevInfo));
    CHECK_MEM(pBlockDevInfo);
    pNvBasicFileSystemFile = NvOsAlloc(sizeof(NvBasicFileSystemFile));
    CHECK_MEM(pNvBasicFileSystemFile);

    // Add file name in file handle
    FileNameLength = NvOsStrlen(FilePath);
    pNvBasicFileSystemFile->pFileName = NvOsAlloc(FileNameLength + 1);
    CHECK_MEM(pNvBasicFileSystemFile->pFileName);
    NvOsStrncpy(pNvBasicFileSystemFile->pFileName,
        FilePath, FileNameLength + 1);
    pNvBasicFileSystemFile->pFileName[FileNameLength] = '\0';

    pNvBasicFileSystemFile->CachedBufferSize = 0;
    hBlkDev = pNvBasicFileSystem->hBlkDevHandle;
    hBlkDev->NvDdkBlockDevGetDeviceInfo(hBlkDev,pBlockDevInfo);
    if (!pBlockDevInfo->TotalBlocks)
    {
        e = NvError_DeviceNotFound;
        NV_DEBUG_PRINTF(("BasicFS FileOpen: Either device is not present "
                            "or its not responding. File open failed \n"));
        goto fail;
    }

    pNvBasicFileSystemFile->BytesPerSector = pBlockDevInfo->BytesPerSector;
    pNvBasicFileSystemFile->SectorNum =
                (NvU32)pNvBasicFileSystem->hPartion->StartLogicalSectorAddress;

    pNvBasicFileSystemFile->pFileCachedBuffer =
        NvOsAlloc(pNvBasicFileSystemFile->BytesPerSector);
    CHECK_MEM(pNvBasicFileSystemFile->pFileCachedBuffer);
    pNvBasicFileSystemFile->pBasicFileSystem = (NvBasicFileSystem*)hFileSystem;
    pNvBasicFileSystemFile->OpenMode = Flags;
    pNvBasicFileSystemFile->FileOffset = 0;
    pNvBasicFileSystemFile->MaxFileSize = ((pBlockDevInfo->BytesPerSector) *
                            (pNvBasicFileSystem->hPartion->NumLogicalSectors));

    pNvBasicFileSystemFile->hFileSystemFile.NvFileSystemFileClose =
                                                    NvFileSystemFileClose;
    pNvBasicFileSystemFile->hFileSystemFile.NvFileSystemFileRead =
                                                    NvFileSystemFileRead;
    pNvBasicFileSystemFile->hFileSystemFile.NvFileSystemFileWrite =
                                                    NvFileSystemFileWrite;
    pNvBasicFileSystemFile->hFileSystemFile.NvFileSystemFileIoctl =
                                                    NvFileSystemFileIoctl;
    pNvBasicFileSystemFile->hFileSystemFile.NvFileSystemFileSeek =
                                                NvFileSystemFileSeek;
     pNvBasicFileSystemFile->hFileSystemFile.NvFileSystemFtell =
                                                NvFileSystemFtell;
    pNvBasicFileSystemFile->hFileSystemFile.NvFileSystemFileQueryFstat =
                                                NvFileSystemFileQueryFstat;
    // Add File name to FileList
    NV_CHECK_ERROR_CLEANUP(AddToNvBasicFSList(FilePath));
    pNvBasicFileSystem->RefFileCount++;
    *phFile = &pNvBasicFileSystemFile->hFileSystemFile;

    if (pBlockDevInfo)
    {
        NvOsFree(pBlockDevInfo);
        pBlockDevInfo = NULL;
    }
    NvOsMutexUnlock(s_NvBasicFSFileMutex);
    return e;

fail:
    if (pNvBasicFileSystemFile)
    {
        if (pNvBasicFileSystemFile->pFileCachedBuffer)
        {
            NvOsFree(pNvBasicFileSystemFile->pFileCachedBuffer);
            pNvBasicFileSystemFile->pFileCachedBuffer = NULL;
        }
        if (pNvBasicFileSystemFile->pFileName)
        {
            NvOsFree(pNvBasicFileSystemFile->pFileName);
            pNvBasicFileSystemFile->pFileName = NULL;
        }
        NvOsFree(pNvBasicFileSystemFile);
        pNvBasicFileSystemFile = NULL;
    }
    if (pBlockDevInfo)
    {
        NvOsFree(pBlockDevInfo);
        pBlockDevInfo = NULL;
    }
    *phFile = NULL;
    NvOsMutexUnlock(s_NvBasicFSFileMutex);
    return e;
}

NvError
NvBasicFileSystemMount(
    NvPartitionHandle hPart,
    NvDdkBlockDevHandle hDevice,
    NvFsMountInfo *pMountInfo,
    NvU32 FileSystemAttr,
    NvFileSystemHandle *phFileSystem)
{
    NvError e = NvSuccess;
    NvBasicFileSystem *pNvBasicFileSystem = NULL;
    CHECK_PARAM(hPart);
    CHECK_PARAM(hDevice);
    NvOsMutexLock(s_NvBasicFSMutex);
    pNvBasicFileSystem = NvOsAlloc(sizeof(NvBasicFileSystem));
    CHECK_MEM(pNvBasicFileSystem);
    pNvBasicFileSystem->hFileSystem.NvFileSystemUnmount =
                            NvFileSystemUnmount;
    pNvBasicFileSystem->hFileSystem.NvFileSystemFileOpen =
                            NvFileSystemFileOpen;
    pNvBasicFileSystem->hFileSystem.NvFileSystemFileQueryStat =
                            NvFileSystemFileQueryStat;
    pNvBasicFileSystem->hFileSystem.NvFileSystemFileSystemQueryStat =
                            NvFileSystemFileSystemQueryStat;

    pNvBasicFileSystem->hFileSystem.NvFileSystemFormat = NvFileSystemFormat;

    pNvBasicFileSystem->hPartion = hPart;
    pNvBasicFileSystem->hBlkDevHandle = hDevice;
    pNvBasicFileSystem->RefFileCount = 0; //Init the RefFile Count to 0
    *phFileSystem = &pNvBasicFileSystem->hFileSystem;
    NvOsMutexUnlock(s_NvBasicFSMutex);
    return e;

fail:
    phFileSystem = NULL;
    NvOsMutexLock(s_NvBasicFSMutex);
    return e;
}

