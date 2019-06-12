/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file          NvAccelerometer.c
 * @brief         <b>Device Driver for Accelerometer</b>
 *
 * @Description : Implementation of the WinCE Accelerometer driver
 */

#include "nvodm_accelerometer.h"


NvBool
NvOdmAccelOpen(NvOdmAccelHandle* hDevice)
{
    *hDevice = NULL;
    return NV_FALSE;
}


void
NvOdmAccelClose(NvOdmAccelHandle hDevice)
{
}


/**
 * After setting the force threshold, we should remove all of interrupt flag
 * Which may be left from last threshold
 */
NvBool
NvOdmAccelSetIntForceThreshold(NvOdmAccelHandle  hDevice,
                               NvOdmAccelIntType IntType,
                               NvU32             IntNum,
                               NvU32             Threshold)
{
    return NV_FALSE;
}

/**
 * After setting the time threshold, we should remove all of interrupt flag
 * Which may be left from last threshold
 */
NvBool
NvOdmAccelSetIntTimeThreshold(NvOdmAccelHandle  hDevice,
                              NvOdmAccelIntType IntType,
                              NvU32             IntNum,
                              NvU32             Threshold)
{
    return NV_FALSE;
}


/**
 * After enable/disable threshold, we should remove all of interrupt flag
 * Which may be left from last threshold
 */
NvBool
NvOdmAccelSetIntEnable(NvOdmAccelHandle   hDevice,
                       NvOdmAccelIntType  IntType,
                       NvOdmAccelAxisType IntAxis,
                       NvU32              IntNum,
                       NvBool             Toggle)
{
    return NV_FALSE;
}


void
NvOdmAccelWaitInt(NvOdmAccelHandle    hDevice,
                  NvOdmAccelIntType  *IntType,
                  NvOdmAccelAxisType *IntMotionAxis,
                  NvOdmAccelAxisType *IntTapAxis)
{
}


void NvOdmAccelSignal(NvOdmAccelHandle hDevice)
{
}

NvBool
NvOdmAccelGetAcceleration(NvOdmAccelHandle hDevice,
                          NvS32           *AccelX,
                          NvS32           *AccelY,
                          NvS32           *AccelZ)
{
    return NV_FALSE;
}


NvOdmAccelerometerCaps
NvOdmAccelGetCaps(NvOdmAccelHandle hDevice)
{
    NvOdmAccelerometerCaps caps;
    NvOdmOsMemset(&caps, 0, sizeof(NvOdmAccelerometerCaps));

    return caps;
}


NvBool
NvOdmAccelSetSampleRate(NvOdmAccelHandle hDevice, NvU32 SampleRate)
{
    return NV_FALSE;
}


NvBool
NvOdmAccelGetSampleRate(NvOdmAccelHandle hDevice, NvU32 *pSampleRate)
{
    return NV_FALSE;
}

NvBool
NvOdmAccelSetPowerState(NvOdmAccelHandle hDevice, NvOdmAccelPowerType PowerState)
{
    return NV_FALSE;
}

