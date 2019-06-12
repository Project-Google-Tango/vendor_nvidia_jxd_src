/*
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#ifndef NV_ENABLE_DEBUG_PRINTS
#define NV_ENABLE_DEBUG_PRINTS (0)
#endif

#define LOG_NDEBUG 0

#include <errno.h>

#include "nvos.h"

#if ENABLE_NVIDIA_CAMTRACE
#include "nvcamtrace.h"
#endif

#include "nvodm_imager.h"
#include "imager_hal.h"

#if NV_ENABLE_DEBUG_PRINTS
#define HAL_DB_PRINTF(...)  NvOsDebugPrintf(__VA_ARGS__)
#else
#define HAL_DB_PRINTF(...)
#endif

#define GAIN_TO_ISO(x) ((NvU32)((x) * 100))
#define PCL_HUGEFLOAT     (1.0e+20)

static NvBool GetSensorStaticProperties(
    NvOdmImagerHandle hImager,
    NvOdmImagerStaticProperty *pProperties
)
{
    SensorStaticProperties StaticProperty;
    NvBool Status = NV_TRUE;
    NvU32 idx;

    HAL_DB_PRINTF("%s\n", __func__);
    if (!hImager || !hImager->pSensor)
    {
        return NV_FALSE;
    }

    NvOdmOsMemset(&StaticProperty, 0, sizeof(StaticProperty));
    hImager->pSensor->pfnStaticQuery(hImager, &StaticProperty);

    if (StaticProperty.pCap->FlashGUID)
    {
        pProperties->FlashAvailable = NV_TRUE;
    }
    else
    {
        pProperties->FlashAvailable = NV_FALSE;
    }

    if (StaticProperty.pCap->FocuserGUID)
    {
        pProperties->FocuserAvailable = NV_TRUE;
    }
    else
    {
        pProperties->FocuserAvailable = NV_FALSE;
    }

    pProperties->SensorOrientation = StaticProperty.pCap->Orientation;
    pProperties->LensFacing = StaticProperty.pCap->Direction ? NV_FALSE : NV_TRUE;
    pProperties->SensorColorFilterArrangement = StaticProperty.pCap->PixelTypes[0];

    // get num of modes supported
    HAL_DB_PRINTF("%s Mode Num = %d\n", __func__, StaticProperty.NumOfModes);
    if (StaticProperty.NumOfModes > MAX_NUM_SENSOR_MODES)
    {
        NvOsDebugPrintf("%s: Too many modes supported! %d\n", __func__, StaticProperty.NumOfModes);
        Status = NV_FALSE;
        goto GetSensorStaticProperties_End;
    }

    {
        ModeProperties *pModeSettings;
        NvU32 SensorPixClkHz;
        NvF32 MinFrameRate = 1000.0f, MaxFrameRate = 0.0f;
        NvF32 MinExposure = 0.0f, MaxExposure = PCL_HUGEFLOAT;
        NvF32 MinTemp, MaxTemp;

        for (idx = 0; idx < StaticProperty.NumOfModes; idx++) {
            pModeSettings = &StaticProperty.ModeList[idx];

            if (!pModeSettings->PixClkAdjustment)
                pModeSettings->PixClkAdjustment = 1.f;

            SensorPixClkHz = ((StaticProperty.pCap->ClockProfiles[0].ExternalClockKHz * pModeSettings->PllMult * pModeSettings->PixClkAdjustment) /
                              (pModeSettings->PllPreDiv * pModeSettings->PllPosDiv)) * 1000;

            MaxTemp = ((NvF32)StaticProperty.MaxCoarseTime *
                (NvF32)pModeSettings->LineLength + StaticProperty.IntegrationTime) /
                ((NvF32)SensorPixClkHz * 2);
            MinTemp = ((NvF32)StaticProperty.MinCoarseTime *
                (NvF32)pModeSettings->LineLength + StaticProperty.IntegrationTime) /
                ((NvF32)SensorPixClkHz * 2);
            // Compute the Max out of the Min coarse time for each modes
            if (MinExposure < MinTemp)
                MinExposure = MinTemp;
            // Compute the Min out of the Max coarse time for each modes
            if (MaxExposure > MaxTemp)
                MaxExposure = MaxTemp;

            MaxTemp = (NvF32)SensorPixClkHz * 2/ (NvF32)(pModeSettings->MinFrameLength * pModeSettings->LineLength);
            MinTemp = (NvF32)SensorPixClkHz * 2/ (NvF32)(StaticProperty.MaxFrameLength * pModeSettings->LineLength);
            if (MinFrameRate > MinTemp)
                MinFrameRate = MinTemp;
            if (MaxFrameRate < MaxTemp)
                MaxFrameRate = MaxTemp;

            pProperties->ScalerAvailableProcessedSizes.Dimensions[idx] = pModeSettings->ActiveSize;
            pProperties->ScalerAvailableProcessedMinDurations.Values[idx] = (NvU64)(1000000000 / MaxTemp);
            pProperties->ScalerAvailableRawSizes.Dimensions[idx] = pModeSettings->ActiveSize;
            pProperties->ScalerAvailableRawMinDurations.Values[idx] = (NvU64)(1000000000 / MaxTemp);
            pProperties->SensorModeList[idx].ActiveDimensions = pModeSettings->ActiveSize;
            pProperties->SensorModeList[idx].PeakFrameRate = pModeSettings->PeakFrameRate;


            HAL_DB_PRINTF("RawSize %d: %d x %d\n", idx, pProperties->ScalerAvailableRawSizes.Dimensions[idx].width,
                           pProperties->ScalerAvailableRawSizes.Dimensions[idx].height);
            HAL_DB_PRINTF("RawMinDurations %d: %lld\n", idx, pProperties->ScalerAvailableRawMinDurations.Values[idx]);
            HAL_DB_PRINTF("ProcessMinDuration %d: %lld\n", idx, pProperties->ScalerAvailableProcessedMinDurations.Values[idx]);
        }
        pProperties->SensorModeNum = StaticProperty.NumOfModes;
        pProperties->SensorExposureTimeRange[0] = (NvU64)(MinExposure * 1000000000);
        pProperties->SensorExposureTimeRange[1] = (NvU64)(MaxExposure * 1000000000);
        pProperties->SensorMaxFrameDuration = (NvU64)(1000000000 / MinFrameRate);
        pProperties->ScalerAvailableProcessedSizes.Size = idx;
        pProperties->ScalerAvailableProcessedMinDurations.Size = idx;
        pProperties->ScalerAvailableRawSizes.Size = idx;
        pProperties->ScalerAvailableRawMinDurations.Size = idx;
    }

    pProperties->SensorActiveArraySize = StaticProperty.ActiveArraySize;
    pProperties->SensorPixelArraySize = StaticProperty.PixelArraySize;
    pProperties->SensorPhysicalSize = StaticProperty.PhysicalSize;

    // get gain parameters
    pProperties->SensorAvailableSensitivities.Property[0] = GAIN_TO_ISO(StaticProperty.MinGain);
    pProperties->SensorAvailableSensitivities.Property[1] = GAIN_TO_ISO(StaticProperty.MaxGain);
    HAL_DB_PRINTF("Gain (%d - %d)\n", pProperties->SensorAvailableSensitivities.Property[0],
        pProperties->SensorAvailableSensitivities.Property[1]);
    pProperties->SensorAvailableSensitivities.Size = 2;
    pProperties->SensorMaxAnalogSensitivity = GAIN_TO_ISO(StaticProperty.MaxGain);
    HAL_DB_PRINTF("%s Gain Num = %d, MAX Gain = %d\n", __func__,
        pProperties->SensorAvailableSensitivities.Size, pProperties->SensorMaxAnalogSensitivity);

    // get lens parameters
    pProperties->LensAvailableFocalLengths.Size = 1;
    pProperties->LensAvailableFocalLengths.Values[0] = StaticProperty.FocalLength;
    pProperties->LensHyperfocalDistance = StaticProperty.HyperFocal;
    pProperties->LensMinimumFocusDistance = StaticProperty.MinFocusDistance;

    // should this determined by input RAW format
    pProperties->SensorWhiteLevel = 0x3FF;
    pProperties->BlackLevelPattern[0][0] = 0x40;
    pProperties->BlackLevelPattern[0][1] = 0x40;
    pProperties->BlackLevelPattern[1][0] = 0x40;
    pProperties->BlackLevelPattern[1][1] = 0x40;

    /* these sensor properties are not supported currently:
       HotPixelInfoMap, not available
       SensorNoiseModelCoefficients, not available
       SensorBaseGainFactor, not available
    */

GetSensorStaticProperties_End:
    return Status;
}

NvBool
NvOdmImagerGetStaticProperty(
    NvOdmImagerHandle hImager,
    NvU64 ImagerGUID,
    NvOdmImagerStaticProperty *pStatic)
{
    NvU64 FocuserGUID = 0LL;
    NvU64 FlashGUID = 0LL;
    pfnImagerGetHal pfnSensorHal = NULL;
    pfnImagerGetHal pfnFocuserHal = NULL;
    pfnImagerGetHal pfnFlashHal = NULL;
    NvOdmImagerHandle pImager = NULL;
    NvBool Result;
    char buf[9];

    HAL_DB_PRINTF("%s +++, %p, ImagerGUID(%s)\n",
        __func__, hImager, ImagerHalGetGuidStr(ImagerGUID, buf, sizeof(buf)));
    if (hImager)
    {
        pImager = hImager;
        goto GetStaticProperty;
    }

    Result = ImagerHalGuidGet(&ImagerGUID, &FocuserGUID, &FlashGUID, NULL);
    if (Result == NV_FALSE)
        return Result;

    pImager = (NvOdmImager*)NvOdmOsAlloc(sizeof(NvOdmImager) + sizeof(NvOdmImagerSensor) + sizeof(NvOdmImagerSubdevice) * 2);
    if (!pImager)
    {
        NvOsDebugPrintf("%s %d: couldn't allocate memory for an imager\n", __func__, __LINE__);
        return NV_FALSE;
    }

    NvOdmOsMemset(pImager, 0, sizeof(NvOdmImager) + sizeof(NvOdmImagerSensor) + sizeof(NvOdmImagerSubdevice) * 2);

    HAL_DB_PRINTF("real using ImagerGUID '%s'\n", ImagerHalGetGuidStr(ImagerGUID, buf, sizeof(buf)));
    // search real sensors first.
    pfnSensorHal = ImagerHalTableSearch(ImagerGUID, NvOdmImagerDevice_Sensor, NV_FALSE);
    if (!pfnSensorHal)
    {
        NvOsDebugPrintf("%s - cannot get imager hal for %s.\n",
            __func__, ImagerHalGetGuidStr(ImagerGUID, buf, sizeof(buf)));
        return NV_FALSE;
    }

    // Get sensor HAL
    pImager->pSensor = (void *)(pImager + 1);
    Result = pfnSensorHal(pImager);
    if (!Result)
    {
        HAL_DB_PRINTF("%s %d: Sensor ERR\n", __func__, __LINE__);
        goto fail;
    }

    if (FocuserGUID)
    {
        pfnFocuserHal = ImagerHalTableSearch(FocuserGUID, NvOdmImagerDevice_Focuser, NV_FALSE);
        HAL_DB_PRINTF("Using FocuserGUID '%s'\n", ImagerHalGetGuidStr(FocuserGUID, buf, sizeof(buf)));
        if (!pfnFocuserHal)
        {
            NvOsDebugPrintf("%s cannot get focuser HAL for %s\n",
                __func__, ImagerHalGetGuidStr(FocuserGUID, buf, sizeof(buf)));
            goto fail;
        }
        pImager->pFocuser = (void *)(pImager->pSensor + 1);
        Result = pfnFocuserHal(pImager);
        if (!Result)
        {
            HAL_DB_PRINTF("%s %d: Focuser ERR\n", __func__, __LINE__);
            goto fail;
        }
    }

    if (FlashGUID)
    {
        pfnFlashHal = ImagerHalTableSearch(FlashGUID, NvOdmImagerDevice_Flash, NV_FALSE);
        HAL_DB_PRINTF("Using FocuserGUID '%s'\n", ImagerHalGetGuidStr(FlashGUID, buf, sizeof(buf)));
        if (!pfnFlashHal)
        {
            NvOsDebugPrintf("%s cannot get flash HAL for %s\n",
                __func__, ImagerHalGetGuidStr(FlashGUID, buf, sizeof(buf)));
            goto fail;
        }
        pImager->pFlash = (void *)(pImager->pFocuser + 1);
        Result = pfnFlashHal(pImager);
        if (!Result)
        {
            HAL_DB_PRINTF("%s %d: Flash ERR\n", __func__, __LINE__);
            goto fail;
        }
    }

GetStaticProperty:
    if (pImager->pSensor->pfnStaticQuery)
    {
        Result = GetSensorStaticProperties(pImager, pStatic);
        if (!Result)
        {
            HAL_DB_PRINTF("%s: get sensor static failed.\n", __func__, __LINE__);
            goto fail;
        }
    }

    if (pImager->pFocuser && pImager->pFocuser->pfnStaticQuery)
    {
        Result = pImager->pFocuser->pfnStaticQuery(pImager, pStatic);
        if (!Result)
        {
            HAL_DB_PRINTF("%s: get focuser static failed.\n", __func__, __LINE__);
            goto fail;
        }
    }

    if (pImager->pFlash && pImager->pFlash->pfnStaticQuery)
    {
        Result = pImager->pFlash->pfnStaticQuery(pImager, pStatic);
        if (!Result)
        {
            HAL_DB_PRINTF("%s: get flash static failed.\n", __func__, __LINE__);
            goto fail;
        }
    }

    if (!hImager)
    {
        NvOdmOsFree(pImager);
    }
    HAL_DB_PRINTF("%s ---\n", __func__);
    return NV_TRUE;

fail:
    NvOdmOsFree(pImager);
    return NV_FALSE;
}

NvBool
NvOdmImagerGetISPStaticProperty(
    NvOdmImagerHandle hImager,
    NvU64 ImagerGUID,
    NvOdmImagerISPStaticProperty *pStatic)
{
#if NV_ENABLE_DEBUG_PRINTS
    char buf[9];
#endif

    HAL_DB_PRINTF("%s +++, %p, ImagerGUID(%s)\n",
        __func__, hImager, ImagerHalGetGuidStr(ImagerGUID, buf, sizeof(buf)));

    if (pStatic && hImager->pSensor->pfnISPStaticQuery)
    {
        NvOdmOsMemset(pStatic, 0, sizeof(*pStatic));
        hImager->pSensor->pfnISPStaticQuery(hImager, pStatic);
    }
    else
    {
        goto fail;
    }

    HAL_DB_PRINTF("%s ---\n", __func__);
    return NV_TRUE;

fail:
    HAL_DB_PRINTF("%s: get sensor ISP static failed.\n", __func__, __LINE__);
    return NV_FALSE;
}

NvBool
NvOdmImagerGetYUVControlProperty(
    NvOdmImagerHandle hImager,
    NvU64 ImagerGUID,
    NvOdmImagerYUVControlProperty *pControl)
{
#if NV_ENABLE_DEBUG_PRINTS
    char buf[9];
#endif

    HAL_DB_PRINTF("%s +++, %p, ImagerGUID(%s)\n",
        __func__, hImager, ImagerHalGetGuidStr(ImagerGUID, buf, sizeof(buf)));

    if (!pControl || !hImager->pSensor->pfnISPControlQuery)
    {
        HAL_DB_PRINTF("%s no hal to get ISP control peroperties.\n", __func__);
        return NV_FALSE;
    }
    NvOdmOsMemset(pControl, 0, sizeof(*pControl));
    hImager->pSensor->pfnISPControlQuery(hImager, pControl);

    HAL_DB_PRINTF("%s ---\n", __func__);
    return NV_TRUE;
}

NvBool
NvOdmImagerGetYUVDynamicProperty(
    NvOdmImagerHandle hImager,
    NvU64 ImagerGUID,
    NvOdmImagerYUVDynamicProperty *pControl)
{
#if NV_ENABLE_DEBUG_PRINTS
    char buf[9];
#endif

    HAL_DB_PRINTF("%s +++, %p, ImagerGUID(%s)\n",
        __func__, hImager, ImagerHalGetGuidStr(ImagerGUID, buf, sizeof(buf)));

    if (!pControl || !hImager->pSensor->pfnISPDynamicQuery)
    {
        HAL_DB_PRINTF("%s no hal to get ISP dynamic peroperties.\n", __func__);
        return NV_FALSE;
    }
    NvOdmOsMemset(pControl, 0, sizeof(*pControl));
    hImager->pSensor->pfnISPDynamicQuery(hImager, pControl);

    HAL_DB_PRINTF("%s ---\n", __func__);
    return NV_TRUE;
}
