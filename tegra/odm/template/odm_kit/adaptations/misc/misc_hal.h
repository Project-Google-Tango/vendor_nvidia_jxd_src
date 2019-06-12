/*
 * Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
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
 *         Abstraction layer stub for misc. SDIO and UART adaptations</b>
 */

#ifndef INCLUDED_NVODM_MISC_ADAPTATION_HAL_H
#define INCLUDED_NVODM_MISC_ADAPTATION_HAL_H

#include "nvcommon.h"
#include "nvodm_sdio.h"
#include "nvodm_uart.h"
#include "nvodm_query_discovery.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct NvOdmMiscRec *NvOdmMiscHandle;

typedef NvBool (*pfnMiscOpen)(NvOdmMiscHandle);
typedef void   (*pfnMiscClose)(NvOdmMiscHandle);
typedef NvBool (*pfnMiscSetPowerState)(NvOdmMiscHandle, NvBool);

typedef struct NvOdmMiscRec
{
    pfnMiscOpen                        pfnOpen;
    pfnMiscClose                       pfnClose;
    pfnMiscSetPowerState               pfnSetPowerState;
    const NvOdmPeripheralConnectivity *pConn;
    NvU32                              PowerOnRefCnt;
} NvOdmMisc, NvOdmSdio, NvOdmUart;

#ifdef __cplusplus
}
#endif

#endif
