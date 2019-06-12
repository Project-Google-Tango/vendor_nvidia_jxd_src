/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** 
 * @file
 * <b>NVIDIA Tegra ODM Kit:
 *         Abstraction layer stub for Temperature Monitor adaptations</b>
 */

#ifndef INCLUDED_NVODM_TMON_ADAPTATION_HAL_H
#define INCLUDED_NVODM_TMON_ADAPTATION_HAL_H

#include "nvcommon.h"
#include "nvodm_tmon.h"
#include "nvodm_query_discovery.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef NvBool (*pfnTmonInit)(NvOdmTmonDeviceHandle);
typedef void   (*pfnTmonDeinit)(NvOdmTmonDeviceHandle);
typedef NvBool (*pfnTmonTemperatureGet)(NvOdmTmonDeviceHandle, NvOdmTmonZoneID, NvS32*);
typedef void   (*pfnTmonCapabilitiesGet)(NvOdmTmonDeviceHandle, NvOdmTmonZoneID, NvOdmTmonCapabilities*);
typedef void   (*pfnTmonParameterCapsGet)
               (NvOdmTmonDeviceHandle, NvOdmTmonZoneID, NvOdmTmonConfigParam, NvOdmTmonParameterCaps*);
typedef NvBool (*pfnTmonParameterConfig)(NvOdmTmonDeviceHandle, NvOdmTmonZoneID, NvOdmTmonConfigParam, NvS32*);
typedef NvBool (*pfnTmonRun)(NvOdmTmonDeviceHandle, NvOdmTmonZoneID);
typedef NvBool (*pfnTmonStop)(NvOdmTmonDeviceHandle, NvOdmTmonZoneID);
typedef NvOdmTmonIntrHandle
               (*pfnTmonIntrRegister)(NvOdmTmonDeviceHandle, NvOdmTmonZoneID, NvOdmInterruptHandler, void*);
typedef void   (*pfnTmonIntrUnregister)(NvOdmTmonDeviceHandle, NvOdmTmonZoneID, NvOdmTmonIntrHandle);

typedef struct NvOdmTmonDeviceRec
{ 
    pfnTmonInit                 pfnInit;
    pfnTmonDeinit               pfnDeinit;
    pfnTmonTemperatureGet       pfnTemperatureGet;
    pfnTmonCapabilitiesGet      pfnCapabilitiesGet;
    pfnTmonParameterCapsGet     pfnParameterCapsGet;
    pfnTmonParameterConfig      pfnParameterConfig;
    pfnTmonRun                  pfnRun;
    pfnTmonStop                 pfnStop;
    pfnTmonIntrRegister         pfnIntrRegister;
    pfnTmonIntrUnregister       pfnIntrUnregister;

    const NvOdmPeripheralConnectivity*  pConn;
    NvU32                       RefCount;
    void                        *pPrivate;
} NvOdmTmonDevice;

#ifdef __cplusplus
}
#endif

#endif  //INCLUDED_NVODM_TMON_ADAPTATION_HAL_H
