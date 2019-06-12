/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvodm_services.h"
#include "nvrm_keylist.h"

//--Key List List Implementation for ODM-defined Key-Value Pairs--

NvU32 NvOdmServicesGetKeyValue(
    NvOdmServicesKeyListHandle handle,
    NvU32 Key)
{
    return NvRmGetKeyValue((NvRmDeviceHandle)handle, Key);
}

NvBool NvOdmServicesSetKeyValuePair(
    NvOdmServicesKeyListHandle handle,
    NvU32 Key,
    NvU32 Value)
{
    if (NvRmSetKeyValuePair((NvRmDeviceHandle)handle, Key, Value) != NvSuccess)
    {
        return NV_FALSE;
    }
    return NV_TRUE;
}
