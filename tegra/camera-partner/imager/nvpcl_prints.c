/**
 * Copyright (c) 2013-2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


#include "nvcommon.h"
#include "nvpcl_controller.h"
#include "nvpcl_driver_sensor.h"
#include "nvpcl_driver_focuser.h"
#include "nvpcl_driver_flash.h"

#include "nvpcl_prints.h"

void NvPclPrintModuleSetting(NvPclModuleSetting settings) {
    NvPclSensorSettings sensorSettings = settings.Sensor;
    NvPclFocuserSettings focuserSettings = settings.Focuser;
    NvPclFlashSettings flashSettings = settings.Flash;
    NvU8 i = 0;

    NvOsDebugPrintf("%s -- Sensor.ModeID: %u\n",
        __func__, sensorSettings.Mode.Id);
    NvOsDebugPrintf("%s -- Sensor.ClockProfile.ExternalClockKHz: %u\n",
        __func__, sensorSettings.ClockProfile.SensorClock.ExternalClockKHz);
    NvOsDebugPrintf("%s -- Sensor.ClockProfile.ClockMultiplier: %u\n",
        __func__, sensorSettings.ClockProfile.SensorClock.ClockMultiplier);
    NvOsDebugPrintf("%s -- Sensor.ClockProfile.LinesPerSecond: %u\n",
        __func__, sensorSettings.ClockProfile.LinesPerSecond);
    NvOsDebugPrintf("%s -- Sensor.ClockProfile.ReadOutTime: %u\n",
        __func__, sensorSettings.ClockProfile.ReadOutTime);
    NvOsDebugPrintf("%s -- Sensor.FrameRate: %f\n",
        __func__, sensorSettings.FrameRate.FrameRate);
    for(i = 0; i < sensorSettings.ExposureList.NoOfExposures; i++) {
        NvOsDebugPrintf("%s -- Sensor.ExposureList.ExposureVals[%d].ExposureTime: %f\n",
            __func__, i,
            sensorSettings.ExposureList.ExposureVal[i].ET);
        NvOsDebugPrintf("%s -- Sensor.ExposureList.ExposureVals[%d].AnalogGain: [%f, %f, %f, %f]\n",
            __func__, i,
            sensorSettings.ExposureList.ExposureVal[i].AnalogGain[0],
            sensorSettings.ExposureList.ExposureVal[i].AnalogGain[1],
            sensorSettings.ExposureList.ExposureVal[i].AnalogGain[2],
            sensorSettings.ExposureList.ExposureVal[i].AnalogGain[3]);
    }
    if(sensorSettings.ExposureList.NoOfExposures < 1) {
        NvOsDebugPrintf("%s -- Sensor.ExposureList.NoOfExposures is less than 1\n", __func__);
    }

    NvOsDebugPrintf("%s -- Focuser.Locus: %d\n", __func__, focuserSettings.Locus);

    for(i = 0; i < PCL_MAX_FLASH_NUMBER; i++) {
        NvOsDebugPrintf("%s -- Flash.Source[%d].CurrentLevel: %f\n",
            __func__, i, flashSettings.Source[i].CurrentLevel);
    }
    NvOsDebugPrintf("%s -- Flash.SustainTime: %f\n", __func__, flashSettings.SustainTime);

    return;
}

void NvPclPrintRunningModuleInfo(NvPclRunningModuleInfo info) {

    NvOsDebugPrintf("%s -- Function call Timestamp: %llu\n", __func__, NvOsGetTimeUS());

    NvOsDebugPrintf("%s -- pclInfo.SensorMode.PixelInfo.ActiveDimensions.width: %d\n",
        __func__, info.SensorMode.PixelInfo.ActiveDimensions.width);
    NvOsDebugPrintf("%s -- pclInfo.SensorMode.PixelInfo.ActiveDimensions.height: %d\n",
        __func__, info.SensorMode.PixelInfo.ActiveDimensions.height);
    NvOsDebugPrintf("%s -- pclInfo.SensorMode.Limits.MinFrameRate: %f\n",
        __func__, info.SensorMode.Limits.MinFrameRate);
    NvOsDebugPrintf("%s -- pclInfo.SensorMode.Limits.MaxFrameRate: %f\n",
        __func__, info.SensorMode.Limits.MaxFrameRate);
    NvOsDebugPrintf("%s -- pclInfo.SensorMode.Limits.MinExposureTime: %llu\n",
        __func__, info.SensorMode.Limits.MinExposureTime);
    NvOsDebugPrintf("%s -- pclInfo.SensorMode.Limits.MaxExposureTime: %llu\n",
        __func__, info.SensorMode.Limits.MaxExposureTime);
    NvOsDebugPrintf("%s -- pclInfo.SensorMode.Limits.MinAnalogGain: %f\n",
        __func__, info.SensorMode.Limits.MinAnalogGain);
    NvOsDebugPrintf("%s -- pclInfo.SensorMode.Limits.MaxAnalogGain: %f\n",
        __func__, info.SensorMode.Limits.MaxAnalogGain);
    NvOsDebugPrintf("%s -- pclInfo.SensorMode.Limits.MinHDRRatio: %f\n",
        __func__, info.SensorMode.Limits.MinHDRRatio);
    NvOsDebugPrintf("%s -- pclInfo.SensorMode.Limits.MaxHDRRatio: %f\n",
        __func__, info.SensorMode.Limits.MaxHDRRatio);
    NvOsDebugPrintf("%s -- pclInfo.FocuserMode.PositionMin: %d\n",
        __func__, info.FocuserMode.PositionMin);
    NvOsDebugPrintf("%s -- pclInfo.FocuserMode.PositionMax: %d\n",
        __func__, info.FocuserMode.PositionMax);

    NvPclPrintModuleSetting(info.Settings);

    return;
}

void NvPclPrintSensorObject(NvPclSensorObject *pMode) {
    NvOsDebugPrintf("\tId: %u\n", pMode->Id);
    NvOsDebugPrintf("\t  Version: %u\n", pMode->Version);
    NvOsDebugPrintf("\t  PixelInfo:\n");
    NvOsDebugPrintf("\t    Orientation: %d\n", pMode->PixelInfo.Orientation);
    NvOsDebugPrintf("\t    PixelType: %d\n", pMode->PixelInfo.PixelType);
    NvOsDebugPrintf("\t      PCLSENSORPIXELTYPE_IS_HDR: %d\n",
        PCLSENSORPIXELTYPE_IS_HDR(pMode->PixelInfo.PixelType));
    NvOsDebugPrintf("\t    ActiveDimensions: %dx%d\n",
            pMode->PixelInfo.ActiveDimensions.width, pMode->PixelInfo.ActiveDimensions.height);
    NvOsDebugPrintf("\t    ActiveStart: %dx%d\n",
            pMode->PixelInfo.ActiveStart.x, pMode->PixelInfo.ActiveStart.y);
    NvOsDebugPrintf("\t    MinimumBlankTime: %dx%d\n",
            pMode->PixelInfo.MinimumBlankTime.width, pMode->PixelInfo.MinimumBlankTime.height);
    NvOsDebugPrintf("\t    PixelAspectRatio: %f\n", pMode->PixelInfo.PixelAspectRatio);
    NvOsDebugPrintf("\t    CropMode: %d\n", pMode->PixelInfo.CropMode);
    NvOsDebugPrintf("\t    ModeInfo.scale: %fx%f\n",
            pMode->PixelInfo.ModeInfo.scale.x, pMode->PixelInfo.ModeInfo.scale.y);
    NvOsDebugPrintf("\t    ModeInfo.rect: %d, %d, %d, %d\n",
            pMode->PixelInfo.ModeInfo.rect.left, pMode->PixelInfo.ModeInfo.rect.top,
            pMode->PixelInfo.ModeInfo.rect.right, pMode->PixelInfo.ModeInfo.rect.bottom);
    NvOsDebugPrintf("\t    SensorActiveArraySize: %d, %d, %d, %d\n",
            pMode->PixelInfo.SensorActiveArraySize.left, pMode->PixelInfo.SensorActiveArraySize.top,
            pMode->PixelInfo.SensorActiveArraySize.right, pMode->PixelInfo.SensorActiveArraySize.bottom);
    NvOsDebugPrintf("\t    SensorPhysicalSize: %fx%f\n",
            pMode->PixelInfo.SensorPhysicalSize.x, pMode->PixelInfo.SensorPhysicalSize.y);
    NvOsDebugPrintf("\t    SensorWhiteLevel: %d\n", pMode->PixelInfo.SensorWhiteLevel);
    NvOsDebugPrintf("\t    BlackLevelpattern[0][0]: %d\n", pMode->PixelInfo.BlackLevelPattern[0][0]);
    NvOsDebugPrintf("\t    BlackLevelpattern[0][1]: %d\n", pMode->PixelInfo.BlackLevelPattern[0][1]);
    NvOsDebugPrintf("\t    BlackLevelpattern[1][0]: %d\n", pMode->PixelInfo.BlackLevelPattern[1][0]);
    NvOsDebugPrintf("\t    BlackLevelpattern[1][1]: %d\n", pMode->PixelInfo.BlackLevelPattern[1][1]);
    NvOsDebugPrintf("\t  Limits:\n");
    NvOsDebugPrintf("\t    MinFrameRate: %f\n", pMode->Limits.MinFrameRate);
    NvOsDebugPrintf("\t    MaxFrameRate: %f\n", pMode->Limits.MaxFrameRate);
    NvOsDebugPrintf("\t    MinExposureTime: %llu\n", pMode->Limits.MinExposureTime);
    NvOsDebugPrintf("\t    MaxExposureTime: %llu\n", pMode->Limits.MaxExposureTime);
    NvOsDebugPrintf("\t    MinAnalogGain: %f\n", pMode->Limits.MinAnalogGain);
    NvOsDebugPrintf("\t    MaxAnalogGain: %f\n", pMode->Limits.MaxAnalogGain);
    NvOsDebugPrintf("\t    MinDigitalGain: %f\n", pMode->Limits.MinDigitalGain);
    NvOsDebugPrintf("\t    MaxDigitalGain: %f\n", pMode->Limits.MaxDigitalGain);
    NvOsDebugPrintf("\t    MinHDRRatio: %f\n", pMode->Limits.MinHDRRatio);
    NvOsDebugPrintf("\t    MaxHDRRatio: %f\n", pMode->Limits.MaxHDRRatio);
    NvOsDebugPrintf("\t  Interface:\n");
    NvOsDebugPrintf("\t    SensorOdmInterface: %d\n", pMode->Interface.SensorOdmInterface);
    NvOsDebugPrintf("\t    ClockProfile.ExternalClockKHz: %u\n", pMode->Interface.ClockProfile.ExternalClockKHz);
    NvOsDebugPrintf("\t    ClockProfile.ClockMultiplier: %f\n", pMode->Interface.ClockProfile.ClockMultiplier);
    NvOsDebugPrintf("\t    InitialSensorClockRateKHz: %u\n", pMode->Interface.InitialSensorClockRateKHz);
    NvOsDebugPrintf("\t    PLL_Multiplier: %f\n", pMode->Interface.PLL_Multiplier);
}

void NvPclPrintFocuserObject(NvPclFocuserObject *pMode) {
    NvOsDebugPrintf("\tId: %u\n", pMode->Id);
    NvOsDebugPrintf("\t  Version: %u\n", pMode->Version);
    NvOsDebugPrintf("\t  PositionMin: %d\n", pMode->PositionMin);
    NvOsDebugPrintf("\t  PositionMax: %d\n", pMode->PositionMax);
    NvOsDebugPrintf("\t  FocalLength: %f\n", pMode->FocalLength);
    NvOsDebugPrintf("\t  MaxAperture: %f\n", pMode->MaxAperture);
    NvOsDebugPrintf("\t  FNumber: %f\n", pMode->FNumber);
    NvOsDebugPrintf("\t  MinFocusDistance: %f\n", pMode->MinFocusDistance);
}

void NvPclPrintFlashObject(NvPclFlashObject *pMode) {
    NvU8 i = 0;
    NvOsDebugPrintf("\tId: %u\n", pMode->Id);
    NvOsDebugPrintf("\t  Version: %u\n", pMode->Version);
    NvOsDebugPrintf("\t  CurrentLevel: %f\n", pMode->CurrentLevel);
    NvOsDebugPrintf("\t  ColorTemperature: %f %f\n", pMode->ColorTemperature.x, pMode->ColorTemperature.y);
    NvOsDebugPrintf("\t  numFlashCurrentLevels: %u\n", pMode->NumFlashCurrentLevels);
    for(i = 0; i < pMode->NumFlashCurrentLevels; i++) {
        NvOsDebugPrintf("\t    FlashCurrentLevel[%d]: %f\n", i, pMode->FlashCurrentLevel[i]);
    }
    NvOsDebugPrintf("\t  numTorchCurrentLevels: %u\n", pMode->NumTorchCurrentLevels);
    for(i = 0; i < pMode->NumTorchCurrentLevels; i++) {
        NvOsDebugPrintf("\t    TorchCurrentLevel[%d]: %f\n", i, pMode->TorchCurrentLevel[i]);
    }
    NvOsDebugPrintf("\t  FlashChargeDuration: %llu\n", pMode->FlashChargeDuration);
}

void NvPclPrintCameraModuleDefinition(NvPclCameraModule *pModule) {
    NvU32 i = 0;
    NvU32 j = 0;
    NvU32 k = 0;
    NvPclDriver *pDriver = NULL;

    NvOsDebugPrintf(" Name : %s\n", pModule->HwModuleDef.Name);
    NvOsDebugPrintf(" Activated: %s", (pModule->Activated)?"True":"False");
    NvOsDebugPrintf(" Direction: %d", pModule->Direction);

    for(i = 0; i < pModule->NumSubDrivers; i++) {
        pDriver = &pModule->SubDriver[i];
        NvOsDebugPrintf("   Driver[%d] Name: %s\n", i, pDriver->Profile.HwDev.Name);
        NvOsDebugPrintf("     Driver Class: %d\n", pDriver->Profile.Type);
        NvOsDebugPrintf("     IsOldNvOdmDriver: %s\n",
            (pDriver->Profile.IsOldNvOdmDriver)?"True":"False");
        for(j = 0; j < pDriver->Profile.FuseID.NumBytes; j++) {
            NvOsDebugPrintf("\tFuseID.data[%d]: %2X", j, pDriver->Profile.FuseID.Data[j]);
        }
        NvOsDebugPrintf("     Handles:\n");
        NvOsDebugPrintf("\thParentModule: %u\n", pDriver->hParentModule);
        NvOsDebugPrintf("\tpfnPclDriverGetUpdateFun: %u\n", pDriver->pfnPclDriverGetUpdateFun);
        NvOsDebugPrintf("\tpfnPclDriverGetVolatile: %u\n", pDriver->pfnPclDriverGetVolatile);
        NvOsDebugPrintf("\tpfnPclDriverInitialize: %u\n", pDriver->pfnPclDriverInitialize);
        NvOsDebugPrintf("\tpfnPclDriverClose: %u\n", pDriver->pfnPclDriverClose);
        NvOsDebugPrintf("\tpPrivateContext: %u\n", pDriver->pPrivateContext);
        NvOsDebugPrintf("     Properties:\n");
        switch(pDriver->Profile.Type) {
            case(NvPclDriverType_Sensor):
                for(k = 0; k < pDriver->Profile.NumObjects; k++) {
                    NvPclPrintSensorObject(&((NvPclSensorObject*)(pDriver->Profile.ObjectList))[k]);
                }
                break;
            case(NvPclDriverType_Focuser):
                for(k = 0; k < pDriver->Profile.NumObjects; k++) {
                    NvPclPrintFocuserObject(&((NvPclFocuserObject*)(pDriver->Profile.ObjectList))[k]);
                }
                break;
            case(NvPclDriverType_Flash):
                for(k = 0; k < pDriver->Profile.NumObjects; k++) {
                    NvPclPrintFlashObject(&((NvPclFlashObject*)(pDriver->Profile.ObjectList))[k]);
                }
                break;
            case(NvPclDriverType_Rom):
                break;
            case(NvPclDriverType_Other):
                break;
            default:
                NvOsDebugPrintf("%s: Unrecognized driver type\n", __func__);
                break;
        }
    }
}

