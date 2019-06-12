/*
 * Copyright (c) 2008 - 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#ifndef INCLUDED_NV3P_SERVER_UTILS_H
#define INCLUDED_NV3P_SERVER_UTILS_H

#include "nvddk_blockdevmgr.h"
#include "nvboot_bit.h"

/**
 * Convert device type from BootDev specific enum to  blockdev specific enum
 *
 * @param nvblDevId nvboot device type
 * @param nvblDevId pointer to blockdev device type
 *
 * @retval NvSuccess Successful conversion
 * @retval NvError_BadParameter Unknown or unconvertable 3P partition type
 */

NvError
NvBl3pConvertBootToRmDeviceType(
    NvBootDevType bootDevId,
    NvDdkBlockDevMgrDeviceId *blockDevId);

/**
 * Convert device type from 3P-specific enum to RM-specific enum
 *
 * @param pDevNv3p 3P device type
 * @param pDevNvRm pointer to RM device type
 *
 * @retval NvSuccess Successful conversion
 * @retval NvError_BadParameter Unknown or unconvertable 3P device
 */
NvError
NvBl3pConvert3pToRmDeviceType(
    Nv3pDeviceType DevNv3p,
    NvRmModuleID *pDevNvRm);

/**
 * validates whether we have got correct rm device id for a given boot device
 *
 * @param BootDevice boot device type
 * @param RmDeviceId RM device type
 *
 * @retval NV_TRUE Successfully validated
 * @retval NV_FALSE invalid rm device
 */

NvBool
NvBlValidateRmDevice(
     NvBootDevType BootDevice,
     NvRmModuleID RmDeviceId);

/**
 * update bct data to to bct poniter specified in bit
 *
 * @param data, bct data to be copied
 * @param BctSection, section of the bct that need to be updated
 *
 */
void
NvBlUpdateBct(
    NvU8* data,
    NvU32 BctSection);

/**
 *update the customer data in bct
 */
void
NvBlUpdateCustomerDataBct(
    NvU8* data);
#endif //INCLUDED_NV3P_SERVER_UTILS_H
