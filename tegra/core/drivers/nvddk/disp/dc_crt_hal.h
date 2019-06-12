/*
 * Copyright (c) 2008 - 2009 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_DC_CRT_HAL_H
#define INCLUDED_DC_CRT_HAL_H

#include "nvcommon.h"
#include "nvos.h"
#include "nvddk_disp_edid.h"
#include "dc_hal.h"
#include "nvodm_query_discovery.h"

#if defined(__cplusplus)
extern "C"
{
#endif

typedef struct DcCrtRec
{
    NvU64 guid;
    NvOdmPeripheralConnectivity const *conn;
    NvDdkDispEdid *edid;
} DcCrt;

void DcCrtEnable( NvU32 controller, NvBool enable );
void DcCrtListModes( NvU32 *nModes, NvOdmDispDeviceMode *modes );
void DcCrtSetPowerLevel( NvOdmDispDevicePowerLevel level );
void DcCrtGetParameter( NvOdmDispParameter param, NvU32 *val );
NvU64 DcCrtGetGuid( void );
void DcCrtSetEdid( NvDdkDispEdid *edid, NvU32 flags );

#if defined(__cplusplus)
}
#endif

#endif
