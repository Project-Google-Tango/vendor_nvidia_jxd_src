/*
 * Copyright (c) 2008 - 2013 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvddk_blockdevmgr.h"
#include "nv3p.h"
#include "nv3p_server_utils.h"
#include "nv3p_server_private.h"

NvError
NvBl3pConvertBootToRmDeviceType(
    NvBootDevType bootDevId,
    NvDdkBlockDevMgrDeviceId *blockDevId)
{

    Nv3pServerUtilsHal Hal;
    if ( Nv3pServerUtilsGetT30Hal(&Hal)
         || Nv3pServerUtilsGetT1xxHal(&Hal)
       )
    {
        return Hal.ConvertBootToRmDeviceType(bootDevId,blockDevId);
    }

    return NvError_NotSupported;
}

/* ------------------------------------------------------------------------ */
NvError
NvBl3pConvert3pToRmDeviceType(
    Nv3pDeviceType DevNv3p,
    NvRmModuleID *pDevNvRm)
{
    Nv3pServerUtilsHal Hal;
    if ( Nv3pServerUtilsGetT30Hal(&Hal)
         || Nv3pServerUtilsGetT1xxHal(&Hal)
       )
    {
        return Hal.Convert3pToRmDeviceType(DevNv3p,pDevNvRm);
    }
    return NvError_NotSupported;
}


NvBool
NvBlValidateRmDevice
    (NvBootDevType BootDevice,
    NvRmModuleID RmDeviceId)
{
    Nv3pServerUtilsHal Hal;

    if ( Nv3pServerUtilsGetT30Hal(&Hal)
         || Nv3pServerUtilsGetT1xxHal(&Hal)
       )
    {
        return Hal.ValidateRmDevice(BootDevice,RmDeviceId);
    }

        return NV_FALSE;
}


void
NvBlUpdateBct(
    NvU8* data,
    NvU32 BctSection)
{
    Nv3pServerUtilsHal Hal;
    if ( Nv3pServerUtilsGetT30Hal(&Hal)
         || Nv3pServerUtilsGetT1xxHal(&Hal)
       )
    {
        Hal.UpdateBct(data,BctSection);
    }
}

void
NvBlUpdateCustomerDataBct(
        NvU8* data
        )
    {
        Nv3pServerUtilsHal Hal;
        if (Nv3pServerUtilsGetT30Hal(&Hal)
                || Nv3pServerUtilsGetT1xxHal(&Hal))
        {
            if (Hal.UpdateCustDataBct)
                Hal.UpdateCustDataBct(data);
        }
    }
