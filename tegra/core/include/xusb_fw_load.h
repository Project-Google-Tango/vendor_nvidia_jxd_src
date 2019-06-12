/*
 * Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef XUSB_FW_LOAD_INCLUDED_H
#define XUSB_FW_LOAD_INCLUDED_H

#include "nvcommon.h"
#include "nvos.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * Checks the existence of XUSB module
 *
 * @retval NV_TRUE Indicates that XUSB module is bonded out
 * @retval NV_FALSE Indicates that XUSB module is present
 */
NvBool IsXusbBondedOut(void);

/**
 * Initializes TSEC for the generation of OTF key
 *
 * @retval NvSuccess Indicates that the XUSB Firmware has been loaded
 * successfully onto falcon. It also represents that the XUSB firmware address
 * is loaded into PCM scratch-34 register
 *
 * @retval NvError_FileOperationFailed Indicates the absence of DFI partition.
 * @retval NvError_InsufficientMemory Indicates the absernce of memory aperture for
 *                                    XUSB
 * @retval NvError_EndOfFile Indicates error in reading DFI partition data
 */
NvError NvLoadXusbFW(NvAbootHandle hAboot);

#ifdef __cplusplus
}
#endif

#endif // XUSB_FW_LOAD_INCLUDED_H
