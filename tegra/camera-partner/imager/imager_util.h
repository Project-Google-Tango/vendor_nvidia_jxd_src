/*
 * Copyright (c) 2008-2014, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef IMAGER_UTIL_H
#define IMAGER_UTIL_H

#include "imager_hal.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define CHECK_PARAM_SIZE_RETURN_MISMATCH(_s, _t) \
    if ((_s) != (_t)) \
    { \
        NV_ASSERT(!"Bad size"); \
        return NV_FALSE; \
    }

#define CHECK_SENSOR_RETURN_NOT_INITIALIZED(_pContext) \
    do { \
        if (!(_pContext)->SensorInitialized) \
        { \
            return NV_FALSE; \
        } \
    }while(0);

typedef struct DevCtrlReg8Rec
{
    NvU8 RegAddr;
    NvU8 RegValue;
} DevCtrlReg8;

typedef struct DevCtrlReg16Rec
{
    NvU16 RegAddr;
    NvU16 RegValue;
} DevCtrlReg16;

typedef struct SensorSetModeSequenceRec
{
    NvOdmImagerSensorMode Mode;
    DevCtrlReg16 *pSequence;
    void *pModeDependentSettings;
} SensorSetModeSequence;

char*
LoadOverridesFile(
    const char *pFiles[],
    NvU32 len);

NvBool
LoadBlobFile(
    const char *pFiles[],
    NvU32 len,
    NvU8 *pBlob,
    NvU32 blobSize);

// Find sensor mode that matches requested mode most closely
// @param pRequest Requested mode
// @param pModeList Mode list to search
// @param NumModes Number of modes in list
// @return Index of selected mode
NvU32
NvOdmImagerGetBestSensorMode(
    const NvOdmImagerSensorMode* pRequest,
    const SensorSetModeSequence* pModeList,
    NvU32 NumModes);

NvBool
LoadFactoryFromEEPROM(
    NvOdmImagerHandle hImager,
    int camera_fd,
    NvU32 ioctl_eeprom,
    void *pValue);
#if defined(__cplusplus)
}
#endif

#endif  //IMAGER_UTIL_H
