/*
 * Copyright (c) 2006-2008 NVIDIA Corporation.  All Rights Reserved.
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
 *         SDIO Adaptation Interface</b>
 *
 * @b Description: Defines the ODM adaptation interface for SDIO devices.
 * 
 */

#ifndef INCLUDED_NVODM_SDIO_H
#define INCLUDED_NVODM_SDIO_H

#if defined(__cplusplus)
extern "C"
{
#endif

#include "nvodm_services.h"
#include "nverror.h"

/**
 * @defgroup nvodm_sdio SDIO Adaptation Interface
 *
 * This is the SDIO ODM adaptation interface.
 *
 * @ingroup nvodm_adaptation
 * @{
 */
 

/**
 * Defines an opaque handle that exists for each SDIO device in the
 * system, each of which is defined by the customer implementation.
 */
typedef struct NvOdmSdioRec *NvOdmSdioHandle;

/**
 * Gets a handle to the SDIO device.
 *
 * @return A handle to the SDIO device.
 */
NvOdmSdioHandle NvOdmSdioOpen(NvU32 Instance);

/**
 * Closes the SDIO handle. 
 *
 * @param hOdmSdio The SDIO handle to be closed.
 */
void NvOdmSdioClose(NvOdmSdioHandle hOdmSdio);

/**
 * Suspends the SDIO device.
 * @param hOdmSdio The handle to SDIO device.
 * @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
NvBool NvOdmSdioSuspend(NvOdmSdioHandle hOdmSdio);

/**
 * Resumes the SDIO device from suspend mode.
 * @param hOdmSdio The handle to SDIO device.
 * @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
NvBool NvOdmSdioResume(NvOdmSdioHandle hOdmSdio);


#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NVODM_Sdio_H
