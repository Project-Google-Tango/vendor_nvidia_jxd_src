/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvodm_services.h"
#include "nvrm_keylist.h"
#include "nvrm_init.h"


//--Key List List Implementation for ODM-defined Key-Value Pairs--


// Secure OS Resource Manager Device Handle

NvOdmServicesKeyListHandle
NvOdmServicesKeyListOpen(void)
{
    NvRmDeviceHandle hRm;

    NvRmInit(&hRm);
    return (NvOdmServicesKeyListHandle)hRm;
}

void NvOdmServicesKeyListClose(NvOdmServicesKeyListHandle handle)
{
}
