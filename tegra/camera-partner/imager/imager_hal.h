/*
 * Copyright (c) 2008-2014, NVIDIA CORPORATION.  All rights reserved.
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
 *         Abstraction layer stub for imager adaptations</b>
 */

#ifndef INCLUDED_NVODM_IMAGER_ADAPTATION_HAL_H
#define INCLUDED_NVODM_IMAGER_ADAPTATION_HAL_H

#include <sys/types.h>
#include <nvc.h>
#include "nvodm_imager.h"
#include "nvos.h"
#include "nvodm_query_discovery.h"
#include "nvodm_imager_guid.h"
#include <nvc.h>

#if (BUILD_FOR_AOS == 0)
#include <asm/types.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct ModePropertiesRec
{
    NvSize ActiveSize;
    NvF32 PeakFrameRate;
    NvU32 LineLength;
    NvU32 FrameLength;
    NvU32 CoarseTime;
    NvU32 CoarseTimeShort;
    NvU32 MinFrameLength;
    NvU32 MaxFrameLength;
    NvF32 InherentGain;
    NvU32 PllMult;
    NvU32 PllPreDiv;
    NvU32 PllPosDiv;
    NvF32 PixClkAdjustment;
    NvBool SupportsBinningControl;
    NvBool BinningControlEnabled;
} ModeProperties;

typedef struct SensorStaticPropertiesRec
{
    NvOdmImagerCapabilities *pCap;
    NvSize PixelArraySize;
    NvPointF32 PhysicalSize;
    NvRect ActiveArraySize;
    NvU32 MaxFrameLength;
    NvU32 MaxCoarseTime;
    NvU32 MinCoarseTime;
    NvU32 IntegrationTime;
    NvU32 NumOfModes;
    ModeProperties ModeList[MAX_NUM_SENSOR_MODES];
    NvF32 MinGain;
    NvF32 MaxGain;
    NvF32 FocalLength;
    NvF32 MaxAperture;
    NvF32 FNumber;
    NvF32 HyperFocal;
    NvF32 MinFocusDistance;
} SensorStaticProperties;

//  A simple HAL for imager adaptations.  Most of these functions
//  map one-to-one with the ODM interface.

typedef NvBool (*pfnImagerSensorOpen)(NvOdmImagerHandle hImager);

typedef void (*pfnImagerSensorClose)(NvOdmImagerHandle hImager);

typedef void
(*pfnImagerSensorGetCapabilities)(NvOdmImagerHandle hImager,
                            NvOdmImagerCapabilities *pCapabilities);

typedef void
(*pfnImagerSensorListSensorModes)(NvOdmImagerHandle hImager,
                            NvOdmImagerSensorMode *pModes,
                            NvS32 *pNumberOfModes);

typedef NvBool
(*pfnImagerSensorSetSensorMode)(NvOdmImagerHandle hImager,
                            const SetModeParameters *pParameters,
                            NvOdmImagerSensorMode *pSelectedMode,
                            SetModeParameters *pResult);

typedef NvBool
(*pfnImagerSensorSetPowerLevel)(NvOdmImagerHandle hImager,
                          NvOdmImagerPowerLevel PowerLevel);

typedef NvBool
(*pfnImagerSensorSetParameter)(NvOdmImagerHandle hImager,
                         NvOdmImagerParameter Param,
                         NvS32 SizeOfValue,
                         const void *pValue);

typedef NvBool
(*pfnImagerSensorGetParameter)(NvOdmImagerHandle hImager,
                         NvOdmImagerParameter Param,
                         NvS32 SizeOfValue,
                         void *pValue);

typedef void
(*pfnImagerSensorGetPowerLevel)(NvOdmImagerHandle hImager,
                          NvOdmImagerPowerLevel *pPowerLevel);


typedef NvBool (*pfnImagerSubdeviceOpen)(NvOdmImagerHandle hImager);

typedef void (*pfnImagerSubdeviceClose)(NvOdmImagerHandle hImager);

typedef NvBool
(*pfnImagerSubdeviceSetPowerLevel)(NvOdmImagerHandle hImager,
                          NvOdmImagerPowerLevel PowerLevel);

typedef NvBool
(*pfnImagerSubdeviceSetParameter)(NvOdmImagerHandle hImager,
                         NvOdmImagerParameter Param,
                         NvS32 SizeOfValue,
                         const void *pValue);

typedef NvBool
(*pfnImagerSubdeviceGetParameter)(NvOdmImagerHandle hImager,
                         NvOdmImagerParameter Param,
                         NvS32 SizeOfValue,
                         void *pValue);

typedef void
(*pfnImagerSubdeviceGetCapabilities)(NvOdmImagerHandle hImager,
                            NvOdmImagerCapabilities *pCapabilities);

typedef NvBool
(*pfnImagerGetStaticProperties)(NvOdmImagerHandle hImager,
                            SensorStaticProperties *pProperties);

typedef NvBool
(*pfnImagerSubdeviceGetStatic)(NvOdmImagerHandle hImager,
                            NvOdmImagerStaticProperty *pProperties);

typedef NvBool
(*pfnImagerSubdeviceGetISPStatic)(NvOdmImagerHandle hImager,
                            NvOdmImagerISPStaticProperty *pProperties);

typedef NvBool
(*pfnImagerSubdeviceGetISPControl)(NvOdmImagerHandle hImager,
                            NvOdmImagerYUVControlProperty *pProperties);

typedef NvBool
(*pfnImagerSubdeviceGetISPDynamic)(NvOdmImagerHandle hImager,
                            NvOdmImagerYUVDynamicProperty *pProperties);

typedef NvBool
(*pfnImagerGetHal)(NvOdmImagerHandle hImager);


typedef struct NvOdmImagerSensorRec
{
    NvU64                                   GUID;

    pfnImagerSensorOpen               pfnOpen;
    pfnImagerSensorClose              pfnClose;
    pfnImagerSensorGetCapabilities    pfnGetCapabilities;
    pfnImagerSensorListSensorModes    pfnListModes;
    pfnImagerSensorSetSensorMode      pfnSetMode;
    pfnImagerSensorSetPowerLevel      pfnSetPowerLevel;
    pfnImagerSensorGetPowerLevel      pfnGetPowerLevel;
    pfnImagerSensorSetParameter       pfnSetParameter;
    pfnImagerSensorGetParameter       pfnGetParameter;
    pfnImagerGetStaticProperties      pfnStaticQuery;
    pfnImagerSubdeviceGetISPStatic    pfnISPStaticQuery;
    pfnImagerSubdeviceGetISPControl   pfnISPControlQuery;
    pfnImagerSubdeviceGetISPDynamic   pfnISPDynamicQuery;

    void*                             pPrivateContext;
    int                               imagerFd;

} NvOdmImagerSensor;

typedef struct NvOdmImagerSubdeviceRec
{
    NvU64       GUID;

    pfnImagerSubdeviceOpen               pfnOpen;
    pfnImagerSubdeviceClose              pfnClose;
    pfnImagerSubdeviceGetCapabilities    pfnGetCapabilities;
    pfnImagerSubdeviceSetPowerLevel      pfnSetPowerLevel;
    pfnImagerSubdeviceSetParameter       pfnSetParameter;
    pfnImagerSubdeviceGetParameter       pfnGetParameter;
    pfnImagerSubdeviceGetStatic          pfnStaticQuery;

    void*       pPrivateContext;
}NvOdmImagerSubdevice;

typedef struct NvOdmImagerRec
{
    NvOdmImagerSensor        *pSensor;
    NvOdmImagerSubdevice     *pFocuser;
    NvOdmImagerSubdevice     *pFlash;

    void                     *pPrivateContext;

} NvOdmImager;

typedef NvS32
(*pfnNvcParamRd)(NvOdmImagerHandle hImager, struct nvc_param *param);

typedef NvS32
(*pfnNvcParamRdExit)(NvOdmImagerHandle hImager,
                     struct nvc_param *param,
                     int err);

typedef NvS32
(*pfnNvcParamWr)(NvOdmImagerHandle hImager, struct nvc_param *param);

typedef NvS32
(*pfnNvcParamWrExit)(NvOdmImagerHandle hImager,
                     struct nvc_param *param,
                     int err);

typedef NvS32
(*pfnNvcParamIspRd)(NvOdmImagerHandle hImager, NvOdmImagerISPSetting *pIsp);

typedef NvS32
(*pfnNvcParamIspRdExit)(NvOdmImagerHandle hImager,
                        NvOdmImagerISPSetting *pIsp,
                        int err);

typedef NvS32
(*pfnNvcParamIspWr)(NvOdmImagerHandle hImager, NvOdmImagerISPSetting *pIsp);

typedef NvS32
(*pfnNvcParamIspWrExit)(NvOdmImagerHandle hImager,
                        NvOdmImagerISPSetting *pIsp,
                        int err);

typedef NvBool
(*pfnNvcFloat2Gain)(NvF32 x, NvU32 *NewGain, NvF32 MinGain, NvF32 MaxGain);

typedef NvBool
(*pfnNvcGain2Float)(NvU32 x, NvF32 *NewGain, NvF32 MinGain, NvF32 MaxGain);

typedef struct NvcImagerHalRec
{
    pfnNvcParamRd                  pfnParamRd;
    pfnNvcParamRdExit              pfnParamRdExit;
    pfnNvcParamWr                  pfnParamWr;
    pfnNvcParamWrExit              pfnParamWrExit;
    pfnNvcParamIspRd               pfnParamIspRd;
    pfnNvcParamIspRdExit           pfnParamIspRdExit;
    pfnNvcParamIspWr               pfnParamIspWr;
    pfnNvcParamIspWrExit           pfnParamIspWrExit;
    pfnNvcFloat2Gain               pfnFloat2Gain;
    pfnNvcGain2Float               pfnGain2Float;
    const char                     *pCalibration;
    const char                     **pOverrideFiles;
    NvU32                          OverrideFilesCount;
    const char                     **pFactoryBlobFiles;
    NvU32                          FactoryBlobFilesCount;
} NvcImagerHal;

typedef struct NvcImagerHalRec *pNvcImagerHal;

typedef NvBool (*pfnNvcImagerGetHal)(pNvcImagerHal hNvcHal, NvU32 DevIdMinor);

NvBool
NvcImagerHalGet(NvU32 DevId, NvU32 DevIdMinor, NvcImagerHal *hNvcHal);

NvBool
ImagerHalGuidGet(NvU64 *ImagerGUID, NvU64 *FocuserGUID, NvU64 *FlashGUID, NvBool *UseSensorCapGUIDs);

pfnImagerGetHal
ImagerHalTableSearch(NvU64 DevGUID, NvOdmImagerDevice DevType, NvBool VirtualSensor);

char *ImagerHalGetGuidStr(NvU64 guid, char *buf, NvU32 size);

#ifdef __cplusplus
}
#endif

#endif
