/*
 * Copyright (c) 2007-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_nvrm_keylist_H
#define INCLUDED_nvrm_keylist_H


#if defined(__cplusplus)
extern "C"
{
#endif

#include "nvrm_module.h"
#include "nvrm_init.h"

/** @file
 * @brief <b>NVIDIA Driver Development Kit:
 *     Resource Manager Key-List APIs</b>
 *
 * @b Description: This API, defines a simple means to set/get the state
 * of ODM-Defined Keys.
 */

#include "nvos.h"
#include "nvodm_keylist_reserved.h"

/**
 * Searches the List of Keys present and returns
 * the Value of the appropriate Key.
 *
 * @param hRm Handle to the RM Device.
 * @param KeyID ID of the key whose value is required.
 *
 * @retval returns the value of the corresponding key. If the Key is not
 * present in the list, it returns 0.
 */



 NvU32 NvRmGetKeyValue(
    NvRmDeviceHandle hRm,
    NvU32 KeyID );

/**
 * Searches the List of Keys Present and sets the value of the Key to the value
 * given. If the Key is not present, it adds the key to the list and sets the
 * value.
 *
 * @param hRM Handle to the RM Device.
 * @param KeyID ID of the key whose value is to be set.
 * @param Value Value to be set for the corresponding key.
 *
 * @retval NvSuccess Value has been successfully set.
 * @retval NvError_InsufficientMemory Operation has failed while adding the
 * key to the existing list.
 */

 NvError NvRmSetKeyValuePair(
    NvRmDeviceHandle hRm,
    NvU32 KeyID,
    NvU32 Value );

#if defined(__cplusplus)
}
#endif

#endif
