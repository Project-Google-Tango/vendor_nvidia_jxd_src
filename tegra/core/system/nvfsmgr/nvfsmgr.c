/*
 * Copyright (c) 2008-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "nvos.h"
#include "nvassert.h"

#include "nvfsmgr.h"
#include "nvextfsmgr.h"
#include "nvfs.h"
#include "../nvfs/basic/nvbasicfilesystem.h"
#ifdef NV_EMBEDDED_BUILD
#include "../nvfs/enhanced/nvenhancedfilesystem.h"
#include "../nvfs/ext2/nvext2filesystem.h"
#endif

#if (NV_DEBUG)
#define NV_FSMGR_TRACE(x) NvOsDebugPrintf x
#else
#define NV_FSMGR_TRACE(x)
#endif // NV_DEBUG

// placeholder for filesystem drivers info
typedef struct NvFsMgrFsDriverInfoRec
{
    pfNvXxxFileSystemInit Init;
    pfNvXxxFileSystemMount Mount;
    pfNvXxxFileSystemDeinit Deinit;
} NvFsMgrFsDriverInfo;

/******************************************************************************
 * Global Variables
 *****************************************************************************/

static NvFsMgrFsDriverInfo gs_FsDriverInfo[NvFsMgrFileSystemType_Num];
static NvU32 gs_RefCount = 0;

static NvExtFsMgrFsDriverInfo *gs_ExtFsDriverInfo;

/******************************************************************************
 * PUBLIC APIs
 *****************************************************************************/
NvError NvFsMgrInit(void)
{
    NvError Err = NvError_NotInitialized;
    NvU32 i;
    NvU32 FailIdx = 0;
    NvU32 NumExtFs = 0;

    if (gs_RefCount)
    {
        ++gs_RefCount;
        Err = NvSuccess;
        goto StatusReturn;
    }

    // [TODO]: initialize the function pointers using actual function names
    // from nvfs implementations. Currently they are assigned to NULL

    // initialize function pointers of basic file system
    gs_FsDriverInfo[NvFsMgrFileSystemType_Basic].Init = &NvBasicFileSystemInit;
    gs_FsDriverInfo[NvFsMgrFileSystemType_Basic].Mount = &NvBasicFileSystemMount;
    gs_FsDriverInfo[NvFsMgrFileSystemType_Basic].Deinit = &NvBasicFileSystemDeinit;
#ifdef NV_EMBEDDED_BUILD
    // initialize function pointers of enhanced file system
    gs_FsDriverInfo[NvFsMgrFileSystemType_Enhanced].Init = &NvEnhancedFileSystemInit;
    gs_FsDriverInfo[NvFsMgrFileSystemType_Enhanced].Mount = &NvEnhancedFileSystemMount;
    gs_FsDriverInfo[NvFsMgrFileSystemType_Enhanced].Deinit = &NvEnhancedFileSystemDeinit;

    // initialize function pointers of ext2 file system
    gs_FsDriverInfo[NvFsMgrFileSystemType_Ext2].Init = &NvExt2FileSystemInit;
    gs_FsDriverInfo[NvFsMgrFileSystemType_Ext2].Mount = &NvExt2FileSystemMount;
    gs_FsDriverInfo[NvFsMgrFileSystemType_Ext2].Deinit = &NvExt2FileSystemDeinit;

    // initialize function pointers of ext3 file system
    gs_FsDriverInfo[NvFsMgrFileSystemType_Ext3].Init = &NvExt2FileSystemInit;
    gs_FsDriverInfo[NvFsMgrFileSystemType_Ext3].Mount = &NvExt2FileSystemMount;
    gs_FsDriverInfo[NvFsMgrFileSystemType_Ext3].Deinit = &NvExt2FileSystemDeinit;

    // initialize function pointers of ext4 file system
    gs_FsDriverInfo[NvFsMgrFileSystemType_Ext4].Init = &NvExt2FileSystemInit;
    gs_FsDriverInfo[NvFsMgrFileSystemType_Ext4].Mount = &NvExt2FileSystemMount;
    gs_FsDriverInfo[NvFsMgrFileSystemType_Ext4].Deinit = &NvExt2FileSystemDeinit;

    // initialize function pointers of basic file system
    gs_FsDriverInfo[NvFsMgrFileSystemType_Qnx].Init = &NvBasicFileSystemInit;
    gs_FsDriverInfo[NvFsMgrFileSystemType_Qnx].Mount = &NvBasicFileSystemMount;
    gs_FsDriverInfo[NvFsMgrFileSystemType_Qnx].Deinit = &NvBasicFileSystemDeinit;
#endif
    // Initialize file systems
    for (i = NvFsMgrFileSystemType_None + 1; i < NvFsMgrFileSystemType_Num; i++)
    {
        if (gs_FsDriverInfo[i].Init)
        {
            Err = (*gs_FsDriverInfo[i].Init)();
            if (Err != NvSuccess)
            {
                NV_FSMGR_TRACE(("[NvFsMgrInit]: Initialization of Filesystem "
                "Type: %d failed", i));
                FailIdx = i;
                break;
            }
        }
        else
        {
            NV_FSMGR_TRACE(("[NvFsMgrInit]: Could not initialize Filesystem "
                "type: %d since the Init function was NULL", i));
        }
    }

    if( Err != NvSuccess )
    {
        // either there was error initializing a filesystem or none of the 
        // filesystems got initialized
        NV_FSMGR_TRACE(("[NvFsMgrInit]: Error during filesystems initialization"));
        
        // deinitialize all the initialized filesystems
        // if none of the filesystems got initialized FailIdx is 0
        for (i = NvFsMgrFileSystemType_None + 1; i < FailIdx; i++)
        {
            if (gs_FsDriverInfo[i].Deinit)
            {
                (*gs_FsDriverInfo[i].Deinit)();
            }
        }
        goto StatusReturn;
    }

    /** External FS Intialization **/
    NumExtFs = NvExtFsMgrGetNumOfExtFilesystems();
    gs_ExtFsDriverInfo = NvOsAlloc(NumExtFs * sizeof(NvExtFsMgrFsDriverInfo));
    NV_ASSERT(gs_ExtFsDriverInfo);
    if (gs_ExtFsDriverInfo == NULL)
    {
        NV_FSMGR_TRACE(("[NvFsMgrInit]: Memory Allocation failure"));
        Err = NvError_InsufficientMemory;
        goto StatusReturn;
    }
    if (NumExtFs > 0)
    {
        Err = NvExtFsMgrGetExtFilesystemsInfo(gs_ExtFsDriverInfo);

        if (Err == NvSuccess)
        {
            for (i = 0;
                i < NumExtFs;
                i++)
            {
                if (gs_ExtFsDriverInfo[i].Init)
                {
                    Err = (*gs_ExtFsDriverInfo[i].Init)();
                    if (Err != NvSuccess)
                    {
                        NV_FSMGR_TRACE(("[NvFsMgrInit]: Initialization of ExternalFS "
                        "Type: %d failed", i));
                        FailIdx = i;
                        break;
                    }
                }
                else
                {
                    NV_FSMGR_TRACE(("[NvFsMgrInit]: Could not initialize ExternalFS "
                        "type: %d since the Init function was NULL", i));
                }
            }
        }

        if( Err != NvSuccess )
        {
            // either there was error initializing a filesystem or none of the 
            // filesystems got initialized
            NV_FSMGR_TRACE(("[NvFsMgrInit]: Error during ExternalFS initialization"));
            
            // deinitialize all the initialized filesystems
            // if none of the filesystems got initialized FailIdx is 0
            for (i = 0; i < FailIdx; i++)
            {
                if (gs_ExtFsDriverInfo[i].DeInit)
                {
                    (*gs_ExtFsDriverInfo[i].DeInit)();
                }
            }
        }
    }
    if (Err == NvSuccess)
    {
        ++gs_RefCount;
    }
StatusReturn:
    return Err;
}

void NvFsMgrDeinit(void)
{
    NvU32 i;
    NvU32 NumExtFs;

    NV_ASSERT(gs_RefCount);

    if (gs_RefCount)
    {
        --gs_RefCount;
        if (gs_RefCount == 0)
        {
            // Deinitialize all the internal filesystems
            for (i = NvFsMgrFileSystemType_None+1; 
                 i < NvFsMgrFileSystemType_Num; 
                 i++)
            {
                if (gs_FsDriverInfo[i].Deinit)
                {
                    (*gs_FsDriverInfo[i].Deinit)();
                }
                else
                {
                    NV_FSMGR_TRACE(("[NvFsMgrDeinit]: Could not uninitialize "
                        "Filesystem type: %d since the Deinit function was "
                        "NULL", i));
                }
            }

            // uninitialize fs driver info
            NvOsMemset(gs_FsDriverInfo, 0, 
                (NvFsMgrFileSystemType_Num * sizeof(NvFsMgrFsDriverInfo)));

            // Deinitialize all the external filesystems
            NumExtFs = NvExtFsMgrGetNumOfExtFilesystems();
            for (i = 0; 
                 i < NumExtFs; 
                 i++)
            {
                if (gs_ExtFsDriverInfo[i].DeInit)
                {
                    (*gs_ExtFsDriverInfo[i].DeInit)();
                }
                else
                {
                    NV_FSMGR_TRACE(("[NvFsMgrDeinit]: Could not uninitialize "
                        "ExternalFS type: %d since the Deinit function was "
                        "NULL", i));
                }
            }
            NvOsFree(gs_ExtFsDriverInfo);
        }
    }
}

NvError
NvFsMgrFileSystemMount(
    NvFsMgrFileSystemType FsType,
    NvPartitionHandle hPart,
    NvDdkBlockDevHandle hDevice,
    NvFsMountInfo *pFsMountInfo,
    NvU32 FileSystemAttr,
    NvFileSystemHandle *phFs)
{
    NvError e = NvSuccess;
    NvU32 ExtFsOffset=0;
    
    // validate if fs mgr is initialized
    NV_ASSERT(gs_RefCount);
    if (!gs_RefCount)
    {
        e = NvError_NotInitialized;
        NV_FSMGR_TRACE(("[NvFsMgrFileSystemMount]: FS Mgr not initialized"));
        goto fail;
    }
    // validate arguments
    NV_ASSERT(hPart);
    NV_ASSERT(hDevice);
    NV_ASSERT(pFsMountInfo);
    
    if ((!hPart) || 
        (!hDevice) ||
        (!pFsMountInfo))
    {
        e = NvError_BadParameter;
        NV_FSMGR_TRACE(("[NvFsMgrFileSystemMount]: Invalid arguments"));
        goto fail;
    }

    if (FsType < NvFsMgrFileSystemType_External)
    {
        NV_ASSERT((FsType > NvFsMgrFileSystemType_None) && 
            (FsType < NvFsMgrFileSystemType_Num));

        // Bl assumes FsType as Basic if Filesystem type is not known
        if (FsType >= NvFsMgrFileSystemType_Num)
            FsType = NvFsMgrFileSystemType_Basic;

        if ((FsType <= NvFsMgrFileSystemType_None) || 
            (FsType >= NvFsMgrFileSystemType_Num))
        {
            e = NvError_BadParameter;
            NV_FSMGR_TRACE(("[NvFsMgrFileSystemMount]: Invalid arguments"));
            goto fail;
        }
   
        NV_ASSERT(gs_FsDriverInfo[FsType].Mount);
        if (gs_FsDriverInfo[FsType].Mount)
        {
            NV_CHECK_ERROR_CLEANUP((*gs_FsDriverInfo[FsType].Mount)
                    (hPart, hDevice, pFsMountInfo, FileSystemAttr, phFs));
        }
        else
        {
            NV_FSMGR_TRACE(("[NvFsMgrFileSystemMount]: Mount not initialized"));
            e = NvError_NotInitialized;
            goto fail;
        }
    }
    else
    {
        ExtFsOffset = FsType - NvFsMgrFileSystemType_External;
        NV_ASSERT(gs_ExtFsDriverInfo[ExtFsOffset].Mount);
        if (gs_ExtFsDriverInfo[ExtFsOffset].Mount)
        {
            NV_CHECK_ERROR_CLEANUP((*gs_ExtFsDriverInfo[ExtFsOffset].Mount)
                    (hPart, hDevice, pFsMountInfo, FileSystemAttr, phFs));
        }
        else
        {
            NV_FSMGR_TRACE(("[NvFsMgrFileSystemMount]: Ext Fs Mount not initialized"));
            e = NvError_NotInitialized;
        }
    }
fail:
    return e;
}

