/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvodm_powerkeys.h"

typedef struct NvOdmPowerKeysContextRec
{
    NvU32 dummy;
} NvOdmPowerKeysContext;

/* Register for specific events */
NvBool NvOdmPowerKeysOpen(
                NvOdmPowerKeysHandle *phOdm,
                NvOdmOsSemaphoreHandle *phSema,
                NvU32 *pNumOfKeys)
{
    return NV_FALSE;
}

/* Unregister the specific registered events */
void NvOdmPowerKeysClose(NvOdmPowerKeysHandle hOdm)
{
    ; // do nothing
}

/* Returns the status for a specific power key */
NvBool NvOdmPowerKeysGetStatus(NvOdmPowerKeysHandle hOdm, NvOdmPowerKeysStatus *pKeyStatus)
{
    return NV_FALSE;
}

