/*
 * Copyright (c) 2009 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/**
 * @file
 * <b>NVIDIA Tegra ODM Kit:
 *         External File System Manager Interface</b>
 *
 * @b Description: Defines the ODM interface for NVIDIA External file 
 * system manager.
 *
 */
#ifndef INCLUDED_NVEXTFSMGR_H
#define INCLUDED_NVEXTFSMGR_H

#include "nvcommon.h"
#include "nvfs.h"

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * External file system driver info
 */
typedef struct NvExtFsMgrFsDriverInfoRec
{
    /**
     * pointer to external file system's Init function
     */
    pfNvXxxFileSystemInit Init;
    /**
     * pointer to external file system's Mount function
     */
    pfNvXxxFileSystemMount Mount;
    /**
     * pointer to external file system's DeInit function
     */
    pfNvXxxFileSystemDeinit DeInit;
} NvExtFsMgrFsDriverInfo;

/**
 * Gets the number of supported external file systems
 *
 * @retval number of external file systems
 */
NvU32 NvExtFsMgrGetNumOfExtFilesystems(void);

/**
 * Gets the info of all the supported external file systems
 *
 * @param ppFsInfo pointer to array of struct containing file system info
 *
 * @retval NvError_Success if no error else an appropriate error code
 */
NvError 
NvExtFsMgrGetExtFilesystemsInfo(
    NvExtFsMgrFsDriverInfo *pFsInfo);

/** @}*/

#if defined(__cplusplus)
}
#endif

#endif  // INCLUDED_NVEXTFSMGR_H

