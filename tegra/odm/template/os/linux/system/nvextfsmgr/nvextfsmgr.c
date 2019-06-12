/**
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 *`
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvextfsmgr.h"
#include "../extfs/nvextfilesystem.h"

/**
 * @defgroup nvodm_extfsmgr External File System Manager Query Interface
 * This is the ODM interface for External File System Manager.
 * @ingroup nvodm_extfsmgr
 * @{
 */

/*
 * NvOdmExtFsMgrFileSystemType
 *
 * Enumerated list of external file system types.defined by ODM
 */
typedef enum
{
    /* Add external file system types here */
    NvExtFsMgrFileSystemType_Basic = 0,
    NvExtFsMgrFileSystemType_Num,  /* Total number of extrnal filesystems */
    NvExtFsMgrFileSystemType_Force32 = 0x7FFFFFFF
} NvExtFsMgrFileSystemType;


NvU32 NvExtFsMgrGetNumOfExtFilesystems(void)
{
    return NvExtFsMgrFileSystemType_Num;
}


NvError 
NvExtFsMgrGetExtFilesystemsInfo(
    NvExtFsMgrFsDriverInfo *pFsInfo)
{
    pFsInfo[NvExtFsMgrFileSystemType_Basic].Init = &NvExternalFileSystemInit;
    pFsInfo[NvExtFsMgrFileSystemType_Basic].Mount = &NvExternalFileSystemMount;
    pFsInfo[NvExtFsMgrFileSystemType_Basic].DeInit = &NvExternalFileSystemDeinit;
    return NvSuccess;
}

