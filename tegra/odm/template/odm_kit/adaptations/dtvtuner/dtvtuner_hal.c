/*
 * Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
 
#include "nvcommon.h"
#include "nvodm_dtvtuner.h"
#include "nvodm_services.h"
#include "nvodm_query_discovery.h"
#include "dtvtuner_hal.h"

#define IS_VALID_HANDLE(X) (((NvU32)(X)) & 0x80000000UL)
#define HANDLE_INDEX(X)    (((NvU32)(X)) & 0x7FFFFFFFUL)
#define HANDLE(X) ((NvOdmDtvtunerHandle)((X) | 0x80000000UL))

static NvOdmDtvtunerHandle
GetDtvInstance(NvOdmDtvtunerHandle PseudoHandle)
{
    //  Only a single DTV tuner is supported per system
    static NvOdmDtvtuner DtvInstance;
    static NvBool first = NV_TRUE;

    if (first)
    {
        NvOdmOsMemset(&DtvInstance, 0, sizeof(DtvInstance));
        first = NV_FALSE;
    }

    if (IS_VALID_HANDLE(PseudoHandle) && (HANDLE_INDEX(PseudoHandle)==0))
        return (NvOdmDtvtunerHandle)&DtvInstance;

    return NULL;
}


NvOdmDtvtunerHandle
NvOdmDtvtunerOpen(void)
{
    NvOdmDtvtunerHandle DtvHandle = NULL;

    DtvHandle = GetDtvInstance(HANDLE(0));
    //  iterate over known GUIDs here, and fill in the HAL if a known DTV
    //  tuner GUID is found on the system
    if (DtvHandle->pfnOpen)
    {
        if (DtvHandle->pfnOpen(DtvHandle))
            return HANDLE(0);
    }
        
    return NULL;
}

void
NvOdmDtvtunerClose(NvOdmDtvtunerHandle* phDTVTuner)
{
    NvOdmDtvtunerHandle DtvHandle = NULL;

    if (!phDTVTuner)
        return;

    DtvHandle = GetDtvInstance(*phDTVTuner);

    if (DtvHandle)
    {
        if (DtvHandle->pfnClose)
            DtvHandle->pfnClose(DtvHandle);

        NvOdmOsMemset(DtvHandle, 0, sizeof(NvOdmDtvtuner));
    }
    *phDTVTuner = NULL;
}

void
NvOdmDtvtunerSetPowerLevel(NvOdmDtvtunerHandle hDTVTuner,
                           NvOdmDtvtunerPowerLevel PowerLevel)
{
      NvOdmDtvtunerHandle DtvHandle = NULL;
      DtvHandle = GetDtvInstance(hDTVTuner);

      if (DtvHandle && DtvHandle->pfnSetPowerLevel)
          DtvHandle->pfnSetPowerLevel(DtvHandle, PowerLevel);
}

NvBool
NvOdmDtvtunerSetParameter(NvOdmDtvtunerHandle hDtvtuner,
                          NvOdmDtvtunerParameter Param,
                          NvU32 SizeOfValue,
                          const void *pValue)
{
    NvOdmDtvtunerHandle DtvHandle = NULL;
    DtvHandle = GetDtvInstance(hDtvtuner);
    if (DtvHandle && DtvHandle->pfnSetParam)
        return DtvHandle->pfnSetParam(DtvHandle, Param, SizeOfValue, pValue);

    return NV_FALSE;
}

NvBool
NvOdmDtvtunerGetParameter(NvOdmDtvtunerHandle hDtvtuner,
                          NvOdmDtvtunerParameter Param,
                          NvU32 SizeOfValue,
                          void *pValue)
{
    NvOdmDtvtunerHandle DtvHandle = NULL;
    DtvHandle = GetDtvInstance(hDtvtuner);
    if (DtvHandle && DtvHandle->pfnGetParam)
        return DtvHandle->pfnGetParam(DtvHandle, Param, SizeOfValue, pValue);

    return NV_FALSE;
}
