/*
 * Copyright (c) 2012-2013, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#include "nvcommon.h"
#include "nv3p_transport.h"
#include "nvodm_query.h"
#include "nvaboot_usbf.h"

#ifdef LPM_BATTERY_CHARGING
Nv3pTransportHandle hTrans = NULL;

NvBool
NvBlDetectUsbCharger(void)
{
    Nv3pTransportSetChargingMode(NV_TRUE);
    return Nv3pTransportUsbfCableConnected(NULL, 0);
}

NvError NvBlUsbfGetChargerType(NvOdmUsbChargerType *chargerType)
{
    NvError err = NvSuccess;
    if (!hTrans)
        err = Nv3pTransportOpen(&hTrans, Nv3pTransportMode_default, 0);

    if(err != NvSuccess)
        NvOsDebugPrintf("Cannot open controller and enumerate device\n");
    else
        *chargerType = Nv3pTransportUsbfGetChargerType(&hTrans, 0);
    return err;
}

void NvBlUsbClose(void)
{
    if (!hTrans)
    {
        NvOsDebugPrintf("Error: handle is null\n");
        return;
    }
    Nv3pTransportClose(hTrans);
    hTrans = NULL;
}
#endif
