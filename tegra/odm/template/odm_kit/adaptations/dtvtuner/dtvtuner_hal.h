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
 *         Abstraction layer stub for digital TV tuner adaptations</b>
 */

#ifndef INCLUDED_NVODM_DTVTUNER_ADAPTATION_HAL_H
#define INCLUDED_NVODM_DTVTUNER_ADAPTATION_HAL_H

#include "nvcommon.h"
#include "nvodm_dtvtuner.h"
#include "nvodm_query_discovery.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef NvBool (*pfnDtvOpen)(NvOdmDtvtunerHandle);
typedef void   (*pfnDtvClose)(NvOdmDtvtunerHandle);
typedef void   (*pfnDtvSetPowerLevel)(NvOdmDtvtunerHandle, 
                                    NvOdmDtvtunerPowerLevel);
typedef NvBool (*pfnDtvSetParam)(NvOdmDtvtunerHandle,
                                 NvOdmDtvtunerParameter,
                                 NvU32, const void *);
typedef NvBool (*pfnDtvGetParam)(NvOdmDtvtunerHandle,
                                 NvOdmDtvtunerParameter,
                                 NvU32, void *);

typedef struct NvOdmDtvtunerContextREC
{
    pfnDtvOpen          pfnOpen;
    pfnDtvClose         pfnClose;
    pfnDtvSetPowerLevel pfnSetPowerLevel;
    pfnDtvSetParam      pfnSetParam;
    pfnDtvGetParam      pfnGetParam;
    void               *pPrivate;
    NvBool              Hal;
} NvOdmDtvtuner;

#ifdef __cplusplus
}
#endif

#endif
