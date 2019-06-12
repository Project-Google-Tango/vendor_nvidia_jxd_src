/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVODM_TOUCH_INT_H
#define INCLUDED_NVODM_TOUCH_INT_H

#include "nvodm_services.h"
#include "nvodm_touch.h"


// Module debug: 0=disable, 1=enable
#define NVODMTOUCH_ENABLE_PRINTF (0)

#if (NV_DEBUG && NVODMTOUCH_ENABLE_PRINTF)
#define NVODMTOUCH_PRINTF(x)   NvOdmOsDebugPrintf x
#else
#define NVODMTOUCH_PRINTF(x)
#endif

#if defined(__cplusplus)
extern "C"
{
#endif
 
typedef struct NvOdmTouchDeviceRec{
    NvBool (*ReadCoordinate)    (NvOdmTouchDeviceHandle hDevice, NvOdmTouchCoordinateInfo *coord);
    NvBool (*EnableInterrupt)   (NvOdmTouchDeviceHandle hDevice, NvOdmOsSemaphoreHandle hInterruptSemaphore);
    NvBool (*HandleInterrupt)   (NvOdmTouchDeviceHandle hDevice);
    NvBool (*GetSampleRate)     (NvOdmTouchDeviceHandle hDevice, NvOdmTouchSampleRate* pTouchSampleRate);
    NvBool (*SetSampleRate)     (NvOdmTouchDeviceHandle hDevice, NvU32 rate);
    NvBool (*PowerControl)      (NvOdmTouchDeviceHandle hDevice, NvOdmTouchPowerModeType mode);
    NvBool (*PowerOnOff)        (NvOdmTouchDeviceHandle hDevice, NvBool OnOff);
    void   (*GetCapabilities)   (NvOdmTouchDeviceHandle hDevice, NvOdmTouchCapabilities* pCapabilities);
    NvBool (*GetCalibrationData)(NvOdmTouchDeviceHandle hDevice, NvU32 NumOfCalibrationData, NvS32* pRawCoordBuffer);
    void   (*Close)             (NvOdmTouchDeviceHandle hDevice);
    NvU16                       CurrentSampleRate;
    NvBool                      OutputDebugMessage;
} NvOdmTouchDevice;



#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NVODM_TOUCH_INT_H

