/*
 * Copyright (c) 2008-2009 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvos.h"
#include "nvassert.h"
#include "nvstormgr.h"
#include "nvstormgr_int.h"
#include "nvfsmgr.h"
#include "nvddk_blockdevmgr.h"


#if (NV_DEBUG)
#define NV_STORMGR_TRACE(x) NvOsDebugPrintf x
#else
#define NV_STORMGR_TRACE(x)
#endif 

// Workaround for SD partial erase support
#ifndef WAR_SD_FORMAT_PART
#define WAR_SD_FORMAT_PART 1
#endif

//Macros for error checkings
#define CHECK_PARAM(x) \
        if (!(x)) \
        { \
           return NvError_BadParameter; \
        }

//This is the head of the single linked list of all the handles and internal data 
//of the mounted FS
static NvStorMgrPartFSList* s_NvStorMgrFSList = NULL;
//Mutex to provide exclusion while accessing link list
static NvOsMutexHandle s_NvStorMgrFSListMutex;

#define PARTNAME_DELIMITER '/'

//Internal Helper function 

/**
 * GetPartitionName()
 *
 * Parses give absolute file path to get partition name using '/' as delimiter char
 *
 * @param pFilePath Absolute file path.
 * @param pPartName Partition name from absolute file path.
 *
 * @retval NvSuccess Partition name.
 * @retval NvError_BadParameter For invalid path.
 */
static NvError GetPartitionName(const char* pFilePath, char *pPartName)
{
    NvError Err = NvSuccess;
    NvU32 Pathlength = 0;
    NvU32 Offset = 0;
    NV_ASSERT(pFilePath);
    NV_ASSERT(pPartName);
    CHECK_PARAM(pFilePath);
    CHECK_PARAM(pPartName);
    Pathlength = NvOsStrlen(pFilePath);
    //get offset for '/' in path
    while ((Offset < Pathlength) && (*(pFilePath + Offset) != PARTNAME_DELIMITER))
    {
        Offset++;
    }
    if ( (Offset != 0) && 
         ((Offset+1)<= NVPARTMGR_MOUNTPATH_NAME_LENGTH ) &&
         (Offset <= Pathlength) )
    {
        NvOsStrncpy(pPartName, pFilePath, Offset);
        pPartName[Offset] = '\0';
    }
    else
        Err = NvError_BadParameter;
    return Err;
}

#if 0  // FIXME -- this routine isn't referenced; compiler warning break build
static NvError GetFileName(char* pFilePath, char* pFileName)
{
    NvError e = NvSuccess;
    char PartName[NVPARTMGR_MOUNTPATH_NAME_LENGTH];
    NvU32 PartNamelength = 0;
    NV_ASSERT(pFilePath);
    NV_ASSERT(pFileName);
    if (pFilePath && pFileName )
    {
        NV_CHECK_ERROR(GetPartitionName(pFilePath, PartName));
        PartNamelength = NvOsStrlen(PartName);
        PartNamelength +=1; //add the offset 1 for delimiter char
        NvOsStrncpy(pFileName, pFilePath + PartNamelength,
           (NvOsStrlen(pFilePath) - PartNamelength ));
    }
    return e;
}
#endif

/**
 * GetPartitionIdFromFileName()
 *
 * Retreives Partition ID for a give absolute file path
 * (Absolute file path is of format "PartitionName/filename" where '/' is delimiter char)
 *
 * @param pFilePath Absolute file path.
 * @param pPartitionID Partition ID for partition in give file name.
 *
 * @retval NvSuccess Partition ID.
 * @retval NvError_BadParameter For invalid path. or invalid partition name
 */
static NvError
GetPartitionIdFromFileName(
    const char* pFileName,
    NvU32* pPartitionID)
{
    NvError e = NvSuccess;
    char PartName[NVPARTMGR_MOUNTPATH_NAME_LENGTH] = {0};
    NV_ASSERT(pFileName);
    NV_ASSERT(pPartitionID);
    if (!pPartitionID || !pFileName)
        return NvError_BadParameter;
    NV_CHECK_ERROR(GetPartitionName(pFileName, PartName));
    NV_CHECK_ERROR(NvPartMgrGetIdByName(PartName, pPartitionID));
    return e;
}

/**
 * IsPartitionFSMounted()
 *
 * checks if a partition reffered by a given Partition ID is currently 
 * mounted by traversing chached list of mounted FS (partitions)
 *
 * @param PartitionID Partition ID to look for 
 * @param pFoundNode Node from cached list for a give Partition ID
 *
 * @retval NV_TRUE if FS (partition) is mounted.
 * @retval NV_FALSE if FS (partition) is not mounted
 */

static NvBool IsPartitionFSMounted(NvU32 PartitionID, NvStorMgrPartFSList** pFoundNode)
{
    NvStorMgrPartFSList* pTempNvStorMgrFSList = NULL;
    pTempNvStorMgrFSList = s_NvStorMgrFSList;
    *pFoundNode = NULL;
    while (pTempNvStorMgrFSList != NULL)
    {
        if (pTempNvStorMgrFSList->PartitionID == PartitionID) 
        {   
            *pFoundNode = pTempNvStorMgrFSList;
            break;
        }
        pTempNvStorMgrFSList = pTempNvStorMgrFSList->pNext;
    }
    return (*pFoundNode == NULL) ? NV_FALSE : NV_TRUE;
}

/**
 * MountPartFS()
 *
 * Mounts an FS (partition) for a given absolute file name. In case FS (partition)
 * is already mounted it will return corresponding node from the cached list of FS.
 * when pCreateflag node is set to true this will result in creating a new node from 
 * newly mounted FS ([partition) or from cached list of already mounted FS (partition)
 *
 * @param pFilePath Absolute file path.
 * @param pPartitionID Partition ID for partition in give file name.
 *
 * @retval NvSuccess If successfully mounted / found mounted and node created if new 
 *  node is requested
 * retval NvError_InsufficientMemory is node creattion fails
 * @retval NvError_BadParameter For invalid path. 
 * @retval Other FS mount errors
 */

static NvError
MountPartFS(
    const char* pFileName,
    NvFileSystemHandle* phFSHandle,
    NvStorMgrPartFSList** pNode,
    NvBool* pCreateflag)
{
    NvError e = NvSuccess;
    NvU32 PartitionId = -1;
    NvStorMgrPartFSList* pFoundNode = NULL;
    NvDdkBlockDevHandle hDevHandle = NULL;
    NvFsMountInfo FsMountInfo;
    NvPartitionHandle hPartHandle = NULL;
    NvFileSystemHandle hFs = NULL;
    NvStorMgrPartFSList* pNewNode = NULL;
    NvDdkBlockDevIoctl_GetPartitionPhysicalSectorsInputArgs InArgs;
    NvDdkBlockDevIoctl_GetPartitionPhysicalSectorsOutputArgs OutArgs;

    NV_ASSERT(pFileName);
    NV_ASSERT(pCreateflag);
    NV_ASSERT(pNode);
    if ((pFileName == NULL) || (phFSHandle == NULL) || (pNode == NULL))
        return NvError_BadParameter;
    *phFSHandle = NULL;
    *pNode = NULL;
    NV_CHECK_ERROR_CLEANUP(GetPartitionIdFromFileName(pFileName, &PartitionId));
    if (IsPartitionFSMounted(PartitionId, &pFoundNode) == NV_FALSE)
    {
        NV_CHECK_ERROR_CLEANUP(NvPartMgrGetFsInfo(PartitionId, &FsMountInfo));
        hPartHandle = NvOsAlloc(sizeof(NvPartInfo));
        NV_ASSERT(hPartHandle);
        if(!hPartHandle)
        {
            e = NvError_InsufficientMemory;
            NV_STORMGR_TRACE(("[nvstormgr]: MountPartFS:NvPartMgrGetFsInfo failed"));
            goto fail;
        }
        NV_CHECK_ERROR_CLEANUP(NvPartMgrGetPartInfo(PartitionId, hPartHandle));
        //get block device handle using FS info
        NV_CHECK_ERROR_CLEANUP(
            NvDdkBlockDevMgrDeviceOpen((NvDdkBlockDevMgrDeviceId)FsMountInfo.DeviceId,
                FsMountInfo.DeviceInstance, PartitionId, &hDevHandle));
        NV_ASSERT(hDevHandle);
        if(!hDevHandle)
        {
            NV_STORMGR_TRACE(("[nvstormgr]: MountPartFS:NvDdkBlockDevMgrDeviceOpen NULL"));
            goto fail;
        }

        // Before mounting update the partition physical addresses
        InArgs.PartitionLogicalSectorStart = (NvU32)hPartHandle->StartLogicalSectorAddress;
        InArgs.PartitionLogicalSectorStop = (NvU32)(hPartHandle->StartLogicalSectorAddress + 
                                                    hPartHandle->NumLogicalSectors);

        e = hDevHandle->NvDdkBlockDevIoctl(
                hDevHandle,
                NvDdkBlockDevIoctlType_GetPartitionPhysicalSectors,
                sizeof(NvDdkBlockDevIoctl_GetPartitionPhysicalSectorsInputArgs),
                sizeof(NvDdkBlockDevIoctl_GetPartitionPhysicalSectorsOutputArgs),
                &InArgs,
                &OutArgs);
        if (e != NvSuccess)
        {
            NV_STORMGR_TRACE(("[nvstormgr]: MountPartFS: "
                "NvDdkBlockDevIoctlType_GetPartitionPhysicalSectors "
                "Failed"));
            goto fail;
        }
        
        hPartHandle->StartPhysicalSectorAddress = (NvU64)OutArgs.PartitionPhysicalSectorStart;
        hPartHandle->EndPhysicalSectorAddress = (NvU64)OutArgs.PartitionPhysicalSectorStop;

        NV_CHECK_ERROR_CLEANUP(
                NvFsMgrFileSystemMount((NvFsMgrFileSystemType)FsMountInfo.FileSystemType,
                hPartHandle,
                hDevHandle, 
                &FsMountInfo,
                FsMountInfo.FileSystemAttr,
                &hFs));
        NV_ASSERT(hFs);
        if (hFs)
        {
            *phFSHandle = hFs;
        }
    }//if (IsPartitionFSMounted(PartitionId, pFoundNode) == NV_FALSE)
    else
    {
        *phFSHandle = pFoundNode->StorMgrFile.hFileSystem;
        if (*pCreateflag == NV_FALSE)
            *pNode = pFoundNode; // give back the found node
    }
    if ((e == NvSuccess) && (*pCreateflag == NV_TRUE))
    {
        //create new node 
        pNewNode = NvOsAlloc(sizeof(NvStorMgrPartFSList));
        NV_ASSERT(pNewNode);
        if (!pNewNode)
        {
            e = NvError_InsufficientMemory;
            *pCreateflag = NV_FALSE;
            NV_STORMGR_TRACE(("[nvstormgr]: newNode memory allocation failed"));
            goto fail;
        }
        if(pFoundNode != NULL)
        {
            pNewNode->StorMgrFile.hPart = pFoundNode->StorMgrFile.hPart;
            pNewNode->StorMgrFile.hDevice = pFoundNode->StorMgrFile.hDevice;
            pNewNode->StorMgrFile.hFileSystem = pFoundNode->StorMgrFile.hFileSystem;
            pNewNode->PartitionID = PartitionId; 
            pNewNode->pNext = NULL;
            *pNode = pNewNode;
            *pCreateflag = NV_TRUE;
        }
        else //create node from new mount and FS info
        {
            pNewNode->StorMgrFile.hPart = hPartHandle;
            pNewNode->StorMgrFile.hDevice = hDevHandle;
            pNewNode->StorMgrFile.hFileSystem = hFs;
            pNewNode->PartitionID = PartitionId; 
            pNewNode->pNext = NULL;
            *pNode = pNewNode;
            *pCreateflag = NV_TRUE;
        }
    }

    return e;
fail:
    NvOsFree(hPartHandle);
    if (hFs)
        hFs->NvFileSystemUnmount(hFs);
    if (hDevHandle)
        hDevHandle->NvDdkBlockDevClose(hDevHandle);
    NvOsFree(pNewNode);
    return e;
}

/**
 * AddToNvStorMgrPartFSList()
 *
 * Adds node to the cache FS list
 *
 * @param pNode node to be added 
 *
 * @retval NvSuccess if success
 * @retval NvError_BadParameter if invalid node
 */

static NvError AddToNvStorMgrPartFSList(NvStorMgrPartFSList* pNode)
{
    NvError Err = NvSuccess;
    NV_ASSERT(pNode);
    CHECK_PARAM(pNode);
    //lock the list so no one alters it untill we are done here
    if (s_NvStorMgrFSList == NULL)
        s_NvStorMgrFSList = pNode;
    else
    {
        pNode->pNext = s_NvStorMgrFSList;
        s_NvStorMgrFSList = pNode;
    }
    return Err;
}

/**
 * RemoveFromNvStorMgrPartFSList()
 *
 * removes node from  the cache FS list
 *
 * @param pNode node to be removed 
 *
 * @retval NvSuccess if success
 * @retval NvError_BadParameter if invalid node
 */

static NvError RemoveFromNvStorMgrPartFSList(NvStorMgrPartFSList* pSrchNode)
{
    NvError Err = NvSuccess;
    NvStorMgrPartFSList* pCurNode = NULL;
    NvStorMgrPartFSList* pPrevNode = NULL;
    NV_ASSERT(pSrchNode);
    NV_ASSERT(s_NvStorMgrFSList);
    if ((pSrchNode == NULL) || (s_NvStorMgrFSList == NULL))
       Err = NvError_BadParameter;
    else
    {
        pCurNode = s_NvStorMgrFSList;
        while ((pCurNode != NULL) && (pCurNode != pSrchNode))
        {
            pPrevNode = pCurNode;
            pCurNode = pCurNode->pNext;
        }
        if (pCurNode == NULL)
            Err = NvError_BadParameter;
        else
        {
            if(pPrevNode == NULL)
                s_NvStorMgrFSList = pCurNode->pNext;
            else
                pPrevNode->pNext = pCurNode->pNext;
        }
    }
    return Err;
}

/**
 * CanUnmountFS()
 *
 * Checks if FS (partition) can be unmounted for a give node partition 
 *
 * @param pNode node corresponding to a partition that needs to be queries for unmounting
 *
 * @retval NV_TRUE if FS (partition) can be unmounted
 * @retval NV_FALSE if FS (partition) cannot be unmounted
 */

static NvBool CanUnmountFS(NvStorMgrPartFSList* pNode)
{
    //just wrapper now but we can do much mode incase of multiple file 
    // and if we need to for unmount checks
    NvU32 PartitionId = -1;
    NvBool Retval = NV_FALSE;
    NvStorMgrPartFSList* pTempNode = NULL;
    NV_ASSERT(pNode);
    if (pNode)
    {
        PartitionId = pNode->PartitionID;
        Retval = IsPartitionFSMounted(PartitionId, &pTempNode);
    }
    return Retval? NV_FALSE : NV_TRUE;
}

/**
 * InNvStorMgrPartFSList()
 *
 * Checks if a given node exist in cached FS list 
 *
 * @param pNode node to be checked in cached FS list
 *
 * @retval NV_TRUE if node exist in cached FS list
 * @retval NV_FALSE if node does not exist cached FS list
 */

static NvBool InNvStorMgrPartFSList(NvStorMgrPartFSList* pNode)
{
    NvStorMgrPartFSList* pTemphFs = NULL;
    NV_ASSERT(pNode);
    if ((pNode == NULL) || (s_NvStorMgrFSList == NULL))
       return NV_FALSE;
    else
    {
        pTemphFs = s_NvStorMgrFSList;
        while ((pTemphFs != NULL)&&(pTemphFs != pNode))
            pTemphFs = pTemphFs->pNext;
    }
    return (pTemphFs == NULL)? NV_FALSE:NV_TRUE;
}

/*
 * Initialize the filesystem manager.
 */
NvError NvStorMgrInit(void)
{
    NvError Err = NvSuccess;
    Err = NvOsMutexCreate(&s_NvStorMgrFSListMutex);
    if (Err != NvSuccess)
        return Err;
    Err = NvFsMgrInit();
    return Err;
}

/*
 *
 * Shutdown the filesystem manager.
 */
void NvStorMgrDeinit(void)
{
    if (s_NvStorMgrFSListMutex)
        NvOsMutexDestroy(s_NvStorMgrFSListMutex);
    NvFsMgrDeinit();
    return;
}

/*
 *
 * Open a file
 *
 */

NvError
NvStorMgrFileOpen(
    const char* pFileName,
    NvU32 Flags,
    NvStorMgrFileHandle* phFile)
{
    NvError Err = NvSuccess;
    NvError FileOpenError = NvSuccess;
    NvU32 PartitionId = -1;
    NvFileSystemHandle hFSHandle = NULL;
    NvFileSystemFileHandle hFile = NULL;
    NvStorMgrPartFSList* pStrMgrNode = NULL;
    NvBool NodeCreateflag = NV_TRUE;
    NV_ASSERT(phFile);
    NV_ASSERT(pFileName);
    *phFile = NULL;
    Err = GetPartitionIdFromFileName(pFileName, &PartitionId);
    if (Err != NvSuccess)
        return NvError_BadParameter;
    //lock the list so no one alters it untill we are done here
    NvOsMutexLock(s_NvStorMgrFSListMutex);
    Err = MountPartFS(pFileName, &hFSHandle, &pStrMgrNode , &NodeCreateflag);
    NV_ASSERT(hFSHandle);
    NV_ASSERT(pStrMgrNode);
    if ((Err == NvSuccess) && pStrMgrNode)
    {
        FileOpenError =
            pStrMgrNode->StorMgrFile.hFileSystem->NvFileSystemFileOpen(
                hFSHandle, pFileName, Flags, &hFile);

        if (hFile)
        {
            pStrMgrNode->StorMgrFile.hFile = hFile;
            //add the node
            AddToNvStorMgrPartFSList(pStrMgrNode);
            *phFile = (NvStorMgrFileHandle)pStrMgrNode;
        }
        else
        {
            Err = pStrMgrNode->StorMgrFile.hFileSystem->NvFileSystemUnmount(
                        pStrMgrNode->StorMgrFile.hFileSystem);

            if (Err == NvSuccess)
            {
                NvOsFree(pStrMgrNode->StorMgrFile.hPart);
                pStrMgrNode->StorMgrFile.hDevice->NvDdkBlockDevClose(
                    pStrMgrNode->StorMgrFile.hDevice);
            }
            NvOsFree(pStrMgrNode);
        }
    }
    //unlock the list
    NvOsMutexUnlock(s_NvStorMgrFSListMutex);
    return FileOpenError;
}

/**
 * Close a file
 *
 */

NvError NvStorMgrFileClose(NvStorMgrFileHandle hStorMgrFH)
{
    NvError Err = NvSuccess;
    NvStorMgrPartFSList* pNode = NULL;

    if( !hStorMgrFH )
    {
        return NvSuccess;
    }

    pNode = (NvStorMgrPartFSList*)hStorMgrFH;
    if (InNvStorMgrPartFSList(pNode) == NV_FALSE)
        return NvError_BadParameter;
    
    //close file 
    Err = hStorMgrFH->hFile->NvFileSystemFileClose(hStorMgrFH->hFile);
    if (Err == NvSuccess)
    {
        //lock the list so no one alters it untill we are done here
        NvOsMutexLock(s_NvStorMgrFSListMutex);
        Err = RemoveFromNvStorMgrPartFSList(pNode);
        if (Err == NvSuccess)
        {
            if (CanUnmountFS(pNode)== NV_TRUE)
            {
                Err = hStorMgrFH->hFileSystem->NvFileSystemUnmount(hStorMgrFH->hFileSystem);
                //close the block driver which was open during mount
                if (Err == NvSuccess)
                {
                    NvOsFree(pNode->StorMgrFile.hPart);
                    pNode->StorMgrFile.hDevice->NvDdkBlockDevClose(
                            pNode->StorMgrFile.hDevice);
                }
            }
            if (pNode)
            {
                NvOsFree(pNode);
            }
            
        }
        //unlock the list
        NvOsMutexUnlock(s_NvStorMgrFSListMutex);
    }
    else
    {
        NV_STORMGR_TRACE(("[nvstormgr]: NvFileSystemFileClose failed"));
    }
    return Err;
}

/**
 *
 * Read data from a file into given buffer
 */

NvError NvStorMgrFileRead(NvStorMgrFileHandle hStorMgrFH, void *pBuffer, 
    NvU32 BytesToRead,NvU32 *BytesRead)
{
    NvError Err = NvSuccess;
    NvStorMgrPartFSList* pNode = NULL;
    NV_ASSERT(hStorMgrFH);
    if ((pBuffer == NULL)||(BytesRead == NULL))
        return NvError_BadParameter;
    pNode = (NvStorMgrPartFSList*)hStorMgrFH;
    if (InNvStorMgrPartFSList(pNode) == NV_FALSE)
        return NvError_BadParameter;
    if (hStorMgrFH)
    {
        //call file read
        Err = hStorMgrFH->hFile->NvFileSystemFileRead(
            hStorMgrFH->hFile, pBuffer, BytesToRead, BytesRead);
        if (Err != NvSuccess)
	{    
            NV_STORMGR_TRACE(("[nvstormgr]: NvFileSystemFileRead failed"));
	}
    }
    return Err;
}

/**
 *
 * Write data to a file from given buffer
 *
 */
NvError NvStorMgrFileWrite( NvStorMgrFileHandle hStorMgrFH, void *pBuffer,
      NvU32 BytesToWrite, NvU32 *BytesWritten)
{
    NvError Err = NvSuccess;
    NvStorMgrPartFSList* pNode = NULL;
    NV_ASSERT(hStorMgrFH);
    if ((pBuffer == NULL)||(BytesWritten == NULL) || (hStorMgrFH == NULL))
        return NvError_BadParameter;
    pNode = (NvStorMgrPartFSList*)hStorMgrFH;
    if ( InNvStorMgrPartFSList(pNode) == NV_FALSE)
        return NvError_BadParameter;
    //call file read
    Err = hStorMgrFH->hFile->NvFileSystemFileWrite(
        hStorMgrFH->hFile,
        pBuffer, 
        BytesToWrite, 
        BytesWritten);
    if (Err != NvSuccess)
     {
        NV_STORMGR_TRACE(("[nvstormgr]: NvStorMgrFileWrite  failed"));
     }
    return Err;
}
  
/**
 *
 * Perform an I/O Control operation on the file.
 *
 */
NvError NvStorMgrFileIoctl( NvStorMgrFileHandle hStorMgrFH, NvU32 Opcode,
    NvU32 InputSize, NvU32 OutputSize, const void *InputArgs,
    void *OutputArgs)
{
    NvError Err = NvSuccess;
    NvStorMgrPartFSList* pNode = NULL;
    NV_ASSERT(hStorMgrFH);
    CHECK_PARAM(hStorMgrFH);
    pNode = (NvStorMgrPartFSList*)hStorMgrFH;
    if (InNvStorMgrPartFSList(pNode) == NV_FALSE)
        return NvError_BadParameter;
    Err = hStorMgrFH->hFile->NvFileSystemFileIoctl(
            hStorMgrFH->hFile,(NvFileSystemIoctlType)Opcode,InputSize,
            OutputSize, InputArgs, OutputArgs);
    if (Err != NvSuccess)
     {
        NV_STORMGR_TRACE(("[nvstormgr]: NvFileSystemFileIoctl failed"));
     }
    return Err;
}

/**
 *
 * Query information about a file given its handle.
 *
 */
NvError NvStorMgrFileQueryFstat( NvStorMgrFileHandle hStorMgrFH, NvFileStat* pStat)
{
    NvError Err = NvSuccess;
    NvStorMgrPartFSList* pNode = NULL;
    NV_ASSERT(hStorMgrFH);
    if ((pStat == NULL)||(hStorMgrFH == NULL))
        return NvError_BadParameter;
    pNode = (NvStorMgrPartFSList*)hStorMgrFH;
    if (InNvStorMgrPartFSList(pNode) == NV_FALSE)
        return NvError_BadParameter;
    Err = hStorMgrFH->hFile->NvFileSystemFileQueryFstat(
        hStorMgrFH->hFile,
        pStat);
    if (Err != NvSuccess)
     {
        NV_STORMGR_TRACE(("[nvstormgr]: NvStorMgrFileQueryFstat failed with "));
     }
    return Err;
}

/**
 *
 * Query information about a file given its name.
 *
 */
NvError NvStorMgrFileQueryStat(const char *pFileName, NvFileStat* pStat)
{
    NvError e = NvSuccess;
    NvStorMgrPartFSList* pStrMgrNode  = NULL;
    NvFileSystemHandle hFSHandle = NULL;
    NvBool NodeCreateflag = NV_FALSE;
    NvU32 PartitionId = -1;
    NV_ASSERT(pFileName);
    NV_ASSERT(pStat);
    if ((pFileName == NULL)||(pStat == NULL))
        return NvError_BadParameter;
    NvOsMemset(pStat, 0, sizeof(NvFileStat));
    NV_CHECK_ERROR(GetPartitionIdFromFileName(pFileName, &PartitionId));
    NvOsMutexLock(s_NvStorMgrFSListMutex);
    e = MountPartFS(pFileName, &hFSHandle, &pStrMgrNode , &NodeCreateflag);
    if (e == NvSuccess)
    {
        e = hFSHandle->NvFileSystemFileQueryStat(hFSHandle, pFileName,pStat);
        //node is null means FS was mounter just for this API so unmount FS (partition)
        //node is not mull means FS was given from previouslu mount FS (partition)
        if(pStrMgrNode == NULL) 
            hFSHandle->NvFileSystemUnmount(hFSHandle);
    }
    NvOsMutexUnlock(s_NvStorMgrFSListMutex);
    return e;
}

/**
 *
 * Query information about a filesystem given the name of any file in
 * the filesystem.
 *
 */
NvError
NvStorMgrFileSystemQueryStat(
    const char *pFileName,
    NvFileSystemStat *pStat)
{
    NvError Err = NvSuccess;
    NvStorMgrPartFSList* pStrMgrNode = NULL;
    NvFileSystemHandle hFSHandle = NULL;
    NvBool NodeCreateflag = NV_FALSE;
    NV_ASSERT(pFileName);
    CHECK_PARAM(pFileName);
    //lock the list so no one alters it untill we are done here
    NvOsMutexLock(s_NvStorMgrFSListMutex);
    Err = MountPartFS(pFileName, &hFSHandle, &pStrMgrNode , &NodeCreateflag);
    if ((Err == NvSuccess)&&(hFSHandle))
    {
        Err = hFSHandle->NvFileSystemFileSystemQueryStat(
                hFSHandle, pFileName, pStat);
        //node is null means FS was mounter just for this API so unmount FS (partition)
        //node is not mull means FS was given from previouslu mount FS (partition)
        if(pStrMgrNode == NULL) 
            hFSHandle->NvFileSystemUnmount(hFSHandle);
    }
    NvOsMutexUnlock(s_NvStorMgrFSListMutex);
    return Err;
}

/**
 *
 * Query information about a filesystem given the name of any file in
 * the filesystem.
 *
 */

NvError
NvStorMgrPartitionQueryStat(
    const char *pFileName,
    NvPartitionStat *pStat)
{
    NvError e = NvSuccess;
    NvPartInfo PartInfo;
    NvFsMountInfo FsMountInfo;
    NvDdkBlockDevInfo DeviceInfo;
    NvU32 PartitionId = -1;
    NvDdkBlockDevHandle hDevHandle = NULL;
    NV_ASSERT(pFileName);
    NV_ASSERT(pStat);
    CHECK_PARAM(pFileName);
    CHECK_PARAM(pStat);
    DeviceInfo.BytesPerSector = 0;
    DeviceInfo.SectorsPerBlock = 0;
    DeviceInfo.TotalBlocks = 0;
    NvOsMemset(pStat, 0, sizeof(NvPartitionStat));
    NV_CHECK_ERROR(GetPartitionIdFromFileName(pFileName, &PartitionId));
    NV_CHECK_ERROR(NvPartMgrGetFsInfo(PartitionId, &FsMountInfo));
    NV_CHECK_ERROR(NvPartMgrGetPartInfo(PartitionId,&PartInfo));
    //get block device handle using FS info
    NV_CHECK_ERROR(
    NvDdkBlockDevMgrDeviceOpen((NvDdkBlockDevMgrDeviceId)FsMountInfo.DeviceId,
        FsMountInfo.DeviceInstance, PartitionId, &hDevHandle));
    NV_ASSERT(hDevHandle);
    if(!hDevHandle)
    {
        e = NvError_BadParameter;
        goto fail;
    }
    else
    {
        hDevHandle->NvDdkBlockDevGetDeviceInfo(hDevHandle, &DeviceInfo);
        pStat->PartitionSize = PartInfo.NumLogicalSectors * DeviceInfo.BytesPerSector;
        hDevHandle->NvDdkBlockDevClose(hDevHandle);
    }
fail:
    return e;
}

/**
 *
 * Seeks the file read pointer in file system to a give location 
 * (NOTE: Only supported for file streams opened in read access mode) 
 *
 */
NvError
NvStorMgrFileSeek(NvStorMgrFileHandle hStorMgrFH, NvS64 ByteOffset, NvU32 Whence)
{
    NvError e = NvSuccess;
    NvStorMgrPartFSList* pNode = NULL;
    NV_ASSERT(hStorMgrFH);
    CHECK_PARAM(hStorMgrFH);
    pNode = (NvStorMgrPartFSList*)hStorMgrFH;
    if ( InNvStorMgrPartFSList(pNode) == NV_FALSE)
        return NvError_BadParameter;
    e = hStorMgrFH->hFile->NvFileSystemFileSeek(hStorMgrFH->hFile,
                                                ByteOffset, 
                                                Whence);
    return e;
}
/**
 *
 * Return the file pointer offset from the beginning of the file
 *
 */

NvError
NvStorMgrFtell(NvStorMgrFileHandle hStorMgrFH, NvU64 *ByteOffset)
{
    NvError e = NvSuccess;
    NvStorMgrPartFSList* pNode = NULL;
    NV_ASSERT(hStorMgrFH);
    NV_ASSERT(ByteOffset);
    CHECK_PARAM(hStorMgrFH);
    CHECK_PARAM(ByteOffset);
    pNode = (NvStorMgrPartFSList*)hStorMgrFH;
    if ( InNvStorMgrPartFSList(pNode) == NV_FALSE)
        return NvError_BadParameter;
    e = hStorMgrFH->hFile->NvFileSystemFtell(hStorMgrFH->hFile, 
                                                ByteOffset);
    return e;
}

/**
 *
 * Performs the filesystem level format on the given partition
 *
 */
NvError NvStorMgrFormat(const char * PartitionName)
{
    NvError e = NvSuccess;
    NvFileSystemHandle hFSHandle = NULL;
    NvStorMgrPartFSList* pStrMgrNode = NULL;
    NvBool CreateNode = NV_TRUE;
    NvPartInfo PartInf;

    // Validate input param
    NV_ASSERT(PartitionName);
    CHECK_PARAM(PartitionName);

    NvOsDebugPrintf("\r\nFormat partition %s ", PartitionName);
    // Mount the filesystem for the given partition
    //lock the list so no one alters it untill we are done here
    NvOsMutexLock(s_NvStorMgrFSListMutex);
    e = MountPartFS(PartitionName, &hFSHandle, &pStrMgrNode, &CreateNode);
    NvOsMutexUnlock(s_NvStorMgrFSListMutex);

    NV_ASSERT(hFSHandle);
    NV_ASSERT(pStrMgrNode);

    if ((e == NvSuccess) && (hFSHandle) && (pStrMgrNode))
    {
        NvU32 PartitionId;
        // Partition boundaries need to be passed to block driver for format

#if WAR_SD_FORMAT_PART
        NvPartInfo PartPTInf;
        const char PartitionPTName[] = "PT";
        NvU32 PartitionPTId;
        NvU32 PTendSector;
        NvBool IsPTpartition;
        // FIXME: SD partial erase does not work so using this hack
        // Workaround to tell if previous partition is PT
        // Assumption that partition table is named PT
        // Get Partition id for given partition name
        NV_CHECK_ERROR_CLEANUP(GetPartitionIdFromFileName(PartitionPTName, &PartitionPTId));
        // get partition size and location
        NV_CHECK_ERROR_CLEANUP(NvPartMgrGetPartInfo(PartitionPTId, &PartPTInf));
#endif

        // Get Partition id for given partition name
        NV_CHECK_ERROR_CLEANUP(GetPartitionIdFromFileName(PartitionName, &PartitionId));
        // get partition size and location
        NV_CHECK_ERROR_CLEANUP(NvPartMgrGetPartInfo(PartitionId, &PartInf));

#if WAR_SD_FORMAT_PART
        // FIXME: 
        // We check that previous partition is not PT. If previous 
        // partition is other than PT we allow erase of erasable group for SD
        if (PartitionId == PartitionPTId)
        {
            IsPTpartition = NV_TRUE;
        }
        else 
        {
            IsPTpartition = NV_FALSE;
        }
        PTendSector = (NvU32)(PartPTInf.StartLogicalSectorAddress +
            PartPTInf.NumLogicalSectors);

        // Issue filesystem format
        e = hFSHandle->NvFileSystemFormat(hFSHandle, 
            PartInf.StartLogicalSectorAddress,
            PartInf.NumLogicalSectors, PTendSector, IsPTpartition);
#else
        // Issue filesystem format
        e = hFSHandle->NvFileSystemFormat(hFSHandle, 
            PartInf.StartLogicalSectorAddress,
            PartInf.NumLogicalSectors, 0, NV_FALSE);
#endif
        if (e != NvSuccess)
        {
            NV_STORMGR_TRACE(("[NvStorMgrFormat]: NvFileSystemFormat failed "
                "e = %d", e));
        }
        
        // Do not add the node to store mgr's FS list
        // Check if FS can be unmounted and all resources be released?
        if (CanUnmountFS(pStrMgrNode))
        {
            hFSHandle->NvFileSystemUnmount(hFSHandle);
            NvOsFree(pStrMgrNode->StorMgrFile.hPart);
            pStrMgrNode->StorMgrFile.hDevice->NvDdkBlockDevClose(
                pStrMgrNode->StorMgrFile.hDevice);
        }
        NvOsFree(pStrMgrNode);
    }
    else
    {
        NV_STORMGR_TRACE(("[NvStorMgrFormat]: MountPartFS failed e = %d", 
            e));
    }
fail:
    return e;
}

