/**
 * Copyright (c) 2012-2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


#include "nvpcl_nvodm_tools.h"
#include "nvc.h"

#if NV_ENABLE_DEBUG_PRINTS
#define PCL_DB_PRINTF(...)  NvOsDebugPrintf(__VA_ARGS__)
#else
#define PCL_DB_PRINTF(...)
#endif

static NvError NvPclNvOdmWriteStateSensor(NvPclCameraModuleHandle hModule, NvPclDriver *pDriver, NvPclControlState *pConfigState) {
    NvError status = NvSuccess;
    NvBool result = NV_FALSE;
    NvOdmImagerHandle hImager;
    NvPclModuleSetting *pDesiredSettings;
    NvPclSensorSettings DesiredSensor;
    NvPclSensorObject *pActiveMode;
    NvPclSensorObject *SensorObjectList;
    NvPclDriverProfile *pDriverProfile;

    if((!hModule) || (!pDriver) || (!pConfigState)) {
        NvOsDebugPrintf("%s: Received a Null parameter\n", __func__);
        return NvError_BadParameter;
    }

    pDriverProfile = &pDriver->Profile;
    if(!pDriverProfile->ObjectList)
        return NvError_NotInitialized;

    hImager = hModule->hImager;
    pDesiredSettings = &(pConfigState->InputSettings);
    DesiredSensor = pDesiredSettings->Sensor;
    SensorObjectList = pDriverProfile->ObjectList;
    pDriverProfile->pActiveObject = &(SensorObjectList[pConfigState->OutputSettings.Sensor.Mode.Id]);
    pActiveMode = pDriverProfile->pActiveObject;

    if(PCLSENSORPIXELTYPE_IS_YUV(pActiveMode->PixelInfo.PixelType)) {
        // Do not do anything for NvOdm YUV sensors
        return NvSuccess;
    }

    // Before program sensor rate ot its max, first program the scheme.
    NvU32 FrameRateScheme = DesiredSensor.FrameRateScheme;
    result = NvOdmImagerSetParameter(hImager, NvOdmImagerParameter_FrameRateScheme, sizeof(NvU32), &FrameRateScheme);
    if(!result) {
        NvOsDebugPrintf("%s: NvOdm driver failed to set frame rate scheme\n", __func__);
        status = NvError_NotSupported;
    }

    if(DesiredSensor.FrameRateScheme == NvOdmImagerFrameRateScheme_Imager)
    {
        // scheme to change frame length is from odm,
        // which indirectly adjust frame rate.
        // Use interface to program MAX sensor rate in this scheme.
        NvF32 maxFrameRate = DesiredSensor.FrameRate.FrameRate;
        result = NvOdmImagerSetParameter(hImager, NvOdmImagerParameter_MaxSensorFrameRate, sizeof(NvF32), &maxFrameRate);
        if(!result) {
            NvOsDebugPrintf("%s: NvOdm driver failed to set max Framerate\n", __func__);
            status = NvError_NotSupported;
        }
    }
    else if(DesiredSensor.FrameRateScheme == NvOdmImagerFrameRateScheme_Core)
    {
        // scheme to change frame length is from core,
        // which directly control sensor frame rate.
        // Use interface to program sensor rate in this scheme.
        NvF32 FrameRate = DesiredSensor.FrameRate.FrameRate;
        result = NvOdmImagerSetParameter(hImager, NvOdmImagerParameter_SensorFrameRate, sizeof(NvF32), &FrameRate);
        if(!result) {
            NvOsDebugPrintf("%s: NvOdm driver failed to set new Framerate\n", __func__);
            status = NvError_NotSupported;
        }
    }
    else
    {
        NvOsDebugPrintf("%s: NvOdm driver find unknown frame rate scheme!\n", __func__);
        status = NvError_NotSupported;
    }

    NvBool supportsGroupHold = NV_FALSE;
    result = NvOdmImagerGetParameter(hImager, NvOdmImagerParameter_SensorGroupHold, sizeof(NvBool), &supportsGroupHold);
    if(!result) {
        NvOsDebugPrintf("%s: NvOdm driver failed to read group hold capabilitiy\n", __func__);
        status = NvError_NotSupported;
    }

    /**
     * Redoing exposure values does more good than harm for now
     * regardless of a mode change that should have written some of the values
     * Drivers have logic that check if values are the same as previous state
     * Avoiding the assumption of using if(modeChanged) because of current HDR logic
     */

    if(supportsGroupHold) { // NvOdm drivers currently do not support HDR with group hold
        NvOdmImagerSensorAE groupAE;
        groupAE.ET_enable = NV_TRUE;
        groupAE.ET = DesiredSensor.ExposureList.ExposureVal[0].ET;
        groupAE.gains_enable = NV_TRUE;
        NvOsMemcpy(groupAE.gains, DesiredSensor.ExposureList.ExposureVal[0].AnalogGain, sizeof(NvF32)*4);
        groupAE.HDRRatio_enable = NV_FALSE;
        groupAE.HDRRatio = 0;

        if(DesiredSensor.ExposureList.NoOfExposures == 2) {
            //HDRRatio is long/short
            groupAE.HDRRatio = DesiredSensor.ExposureList.ExposureVal[0].ET/DesiredSensor.ExposureList.ExposureVal[1].ET;
            groupAE.HDRRatio_enable = NV_TRUE;
        } else if(DesiredSensor.ExposureList.NoOfExposures > 2) {
            NvOsDebugPrintf("%s: NvOdm drivers currently do not support HDR exposures with more than 2 values\n", __func__);
            status = NvError_BadParameter;
        }

        result = NvOdmImagerSetParameter(hImager, NvOdmImagerParameter_SensorGroupHold, sizeof(NvOdmImagerSensorAE), &groupAE);
        if(!result) {
            NvOsDebugPrintf("%s: NvOdm driver failed to set group hold\n", __func__);
            status = NvError_NotSupported;
        }
    } else {
        NvF32 gains[4];
        NvOsMemcpy(gains, DesiredSensor.ExposureList.ExposureVal[0].AnalogGain, sizeof(NvF32)*4);
        result = NvOdmImagerSetParameter(hImager, NvOdmImagerParameter_SensorGain, 4*sizeof(NvF32), &gains);
        if(!result) {
            NvOsDebugPrintf("%s: NvOdm driver failed to set gains\n", __func__);
            status = NvError_NotSupported;
        }

        NvF32 exposuretime = DesiredSensor.ExposureList.ExposureVal[0].ET;
        result = NvOdmImagerSetParameter(hImager, NvOdmImagerParameter_SensorExposure, sizeof(NvF32), &exposuretime);
        if(!result) {
            NvOsDebugPrintf("%s: NvOdm driver failed to set exposure time\n", __func__);
            status = NvError_NotSupported;
        }

        if(DesiredSensor.ExposureList.NoOfExposures == 2) {
            //HDRRatio is long/short
            NvF32 HDRRatio = DesiredSensor.ExposureList.ExposureVal[0].ET/DesiredSensor.ExposureList.ExposureVal[1].ET;
            //Setting HDRRatio forces a writeexposure in driver
            result = NvOdmImagerSetParameter(hImager, NvOdmImagerParameter_SensorHDRRatio, sizeof(NvF32), &HDRRatio);
            if(!result) {
                NvOsDebugPrintf("%s: NvOdm driver failed to set new HDR Ratio\n", __func__);
                status = NvError_NotSupported;
            }
        } else if(DesiredSensor.ExposureList.NoOfExposures > 2) {
            NvOsDebugPrintf("%s: NvOdm drivers currently do not support HDR exposures with more than 2 values\n", __func__);
            status = NvError_BadParameter;
        }
    }


    return status;
}

NvError NvPclNvOdmQueryStateSensor(NvPclDriver *pDriver, NvPclModuleSetting *pOutputSettings) {
    NvError status = NvSuccess;
    NvBool result = NV_FALSE;
    NvF32 limits[2];
    NvF32 val_f32;
    NvF32 val_f32x4[4];
    NvOdmImagerHandle hImager;
    NvPclSensorSettings *pCurrentSensor;
    NvPclSensorObject *SensorObjectList;
    NvPclSensorObject *pActiveMode;

    if((!pDriver) || (!pOutputSettings)) {
        NvOsDebugPrintf("%s: Received a Null parameter\n", __func__);
        return NvError_BadParameter;
    }

    hImager = pDriver->hParentModule->hImager;
    pCurrentSensor = &(pOutputSettings->Sensor);
    SensorObjectList = pDriver->Profile.ObjectList;
    pActiveMode = &(SensorObjectList[pOutputSettings->Sensor.Mode.Id]);

    if(PCLSENSORPIXELTYPE_IS_YUV(pActiveMode->PixelInfo.PixelType)) {
        // Do not do anything for YUV sensors, data query not supported
        return NvSuccess;
    }

    /**
     * Fill in queries about sensor mode limits
     */

    result = NvOdmImagerGetParameter(hImager, NvOdmImagerParameter_SensorExposureLimits, sizeof(limits), limits);
    if(!result) {
        NvOsDebugPrintf("%s: NvOdm driver failed to read ExposureLimits\n", __func__);
        status = NvError_NotSupported;
    }
    // ExposureTimeLimits are in nanoseconds
    pActiveMode->Limits.MinExposureTime = limits[0]*1000000000;
    pActiveMode->Limits.MaxExposureTime = limits[1]*1000000000;

    result = NvOdmImagerGetParameter(hImager, NvOdmImagerParameter_SensorFrameRateLimits, sizeof(limits), limits);
    if(!result) {
        NvOsDebugPrintf("%s: NvOdm driver failed to read Frameratelimits\n", __func__);
        status = NvError_NotSupported;
    }
    pActiveMode->Limits.MinFrameRate = limits[0];
    pActiveMode->Limits.MaxFrameRate = limits[1];

    result = NvOdmImagerGetParameter(hImager, NvOdmImagerParameter_SensorGainLimits, sizeof(limits), limits);
    if(!result) {
        NvOsDebugPrintf("%s: NvOdm driver failed to read gain limits\n", __func__);
        status = NvError_NotSupported;
    }
    pActiveMode->Limits.MinAnalogGain = limits[0];
    pActiveMode->Limits.MaxAnalogGain = limits[1];


    // Digital gain was never granularly supported in NvOdm drivers
    pActiveMode->Limits.MinDigitalGain = 1.0;
    pActiveMode->Limits.MaxDigitalGain = 1.0;

    /**
     * Fill in queries about operating sensor mode
     */

    result = NvOdmImagerGetParameter(hImager, NvOdmImagerParameter_SensorFrameRate, sizeof(val_f32), &val_f32);
    if(!result) {
        NvOsDebugPrintf("%s: NvOdm driver failed to read framerate\n", __func__);
        status = NvError_NotSupported;
    }
    pCurrentSensor->FrameRate.FrameRate = val_f32;

    NvF32 identityx4[] = {1.0, 1.0, 1.0, 1.0};
    NvOsMemcpy(pCurrentSensor->ExposureList.ExposureVal[0].DigitalGain, identityx4, sizeof(NvF32)*4);

    result = NvOdmImagerGetParameter(hImager, NvOdmImagerParameter_SensorGain, sizeof(val_f32x4), val_f32x4);
    if(!result) {
        NvOsDebugPrintf("%s: NvOdm driver failed to read sensor gain\n", __func__);
        status = NvError_NotSupported;
    }
    NvOsMemcpy(pCurrentSensor->ExposureList.ExposureVal[0].AnalogGain, val_f32x4, sizeof(NvF32)*4);

    result = NvOdmImagerGetParameter(hImager, NvOdmImagerParameter_SensorExposure, sizeof(val_f32), &val_f32);
    if(!result) {
        NvOsDebugPrintf("%s: NvOdm driver failed to read sensor exposuretime\n", __func__);
        status = NvError_NotSupported;
    }
    pCurrentSensor->ExposureList.ExposureVal[0].ET = val_f32;

    result = NvOdmImagerGetParameter(hImager, NvOdmImagerParameter_SensorHDRRatio, sizeof(val_f32), &val_f32);
    if(!result) {
        pCurrentSensor->ExposureList.NoOfExposures = 1;
    } else {
        NvF32 HDRRatio;
        pCurrentSensor->ExposureList.NoOfExposures = 2;
        // NvOdm implementation of HDR does not support multiple gain configurations
        NvOsMemcpy(pCurrentSensor->ExposureList.ExposureVal[1].DigitalGain, pCurrentSensor->ExposureList.ExposureVal[0].DigitalGain, sizeof(NvF32)*4);
        NvOsMemcpy(pCurrentSensor->ExposureList.ExposureVal[1].AnalogGain, pCurrentSensor->ExposureList.ExposureVal[0].AnalogGain, sizeof(NvF32)*4);
        // considering the amount of work to plumb/enable HDR patterns and such things in old drivers, it may be easier to write new driver
        HDRRatio = val_f32;
        pCurrentSensor->ExposureList.ExposureVal[1].ET = (pCurrentSensor->ExposureList.ExposureVal[0].ET)/HDRRatio;
    }

    result = NvOdmImagerGetParameter(hImager, NvOdmImagerParameter_LinesPerSecond, sizeof(val_f32), &val_f32);
    if(!result) {
        NvOsDebugPrintf("%s: NvOdm driver failed to read LinesPerSecond\n", __func__);
        status = NvError_NotSupported;
    }
    pCurrentSensor->ClockProfile.LinesPerSecond = val_f32;

    result = NvOdmImagerGetParameter(hImager, NvOdmImagerParameter_SensorActiveRegionReadOutTime, sizeof(val_f32), &val_f32);
    if(!result) {
        NvOsDebugPrintf("%s: NvOdm driver failed to read SensorActiveRegionReadOutTime\n", __func__);
        status = NvError_NotSupported;
    }
    pCurrentSensor->ClockProfile.ReadOutTime = val_f32;

    return status;
}


static NvError NvPclNvOdmWriteStateFocuser(NvPclCameraModuleHandle hModule, NvPclDriver *pDriver, NvPclControlState *pConfigState) {
    NvError status = NvSuccess;
    NvBool result = NV_FALSE;
    NvOdmImagerHandle hImager;
    NvPclModuleSetting *pDesiredSettings;
    NvPclFocuserSettings DesiredFocuser;
    NvPclFocuserContext *pContext = NULL;
    NvPclDriverProfile *pDriverProfile;
    NvS32 DesiredLocus = 0;
    NvS32 locus = 0;

    if((!hModule) || (!pDriver) || (!pConfigState)) {
        NvOsDebugPrintf("%s: Received a Null parameter\n", __func__);
        return NvError_BadParameter;
    }

    pDriverProfile = &pDriver->Profile;
    if(!pDriverProfile->ObjectList)
        return NvError_NotInitialized;

    hImager = hModule->hImager;
    pDesiredSettings = &(pConfigState->InputSettings);
    DesiredFocuser = pDesiredSettings->Focuser;
    pContext = pDriver->pPrivateContext;

    DesiredLocus = pContext->DelayedPositionReq;
    pContext->DelayedPositionReq = DesiredFocuser.Locus;

    result = NvOdmImagerGetParameter(hImager, NvOdmImagerParameter_FocuserLocus, sizeof(locus), &locus);
    if(!result) {
        NvOsDebugPrintf("%s: NvOdm driver failed to read focuser locus\n", __func__);
    } else {
        if(locus == DesiredLocus) {
            return status;
        }
    }
    result = NvOdmImagerSetParameter(hImager, NvOdmImagerParameter_FocuserLocus, sizeof(NvS32), &DesiredLocus);
    if(!result) {
        NvOsDebugPrintf("%s: NvOdm driver failed to set new focuser locus (%d)\n", __func__, DesiredLocus);
        status = NvError_NotSupported;
    }

    return status;
}

NvError NvPclNvOdmQueryStateFocuser(NvPclDriver *pDriver, NvPclModuleSetting *pOutputSettings) {
    //
    //Fill in queries about focuser state
    //
    NvError status = NvSuccess;
    NvBool result = NV_FALSE;
    NvS32 val_s32;
    NvOdmImagerHandle hImager;
    NvPclFocuserSettings *pCurrentFocuser;

    if((!pDriver) || (!pOutputSettings)) {
        NvOsDebugPrintf("%s: Received a Null parameter\n", __func__);
        return NvError_BadParameter;
    }

    hImager = pDriver->hParentModule->hImager;
    pCurrentFocuser = &(pOutputSettings->Focuser);

    result = NvOdmImagerGetParameter(hImager, NvOdmImagerParameter_FocuserLocus, sizeof(val_s32), &val_s32);
    if(!result) {
        NvOsDebugPrintf("%s: NvOdm driver failed to read focuser locus\n", __func__);
        status = NvError_NotSupported;
    }
    pCurrentFocuser->Locus = val_s32;

    return status;
}

#define LEDMASK_OFF (0x00)
#define LEDMASK_ENABLE(x) (0x01 << (x))

static NvError NvPclNvOdmWriteStateFlash(NvPclCameraModuleHandle hModule, NvPclDriver *pDriver, NvPclControlState *pConfigState) {
    NvError status = NvSuccess;
    NvBool result = NV_FALSE;
    NvOdmImagerHandle hImager;
    NvPclModuleSetting *pDesiredSettings;
    NvPclFlashSettings DesiredFlash;
    NvPclDriverProfile *pDriverProfile;
    NvPclFlashObject *FlashObjectList;
    NvOdmImagerFlashSetLevel setLevel;
    NvU8 i = 0;

    if((!hModule) || (!pDriver) || (!pConfigState)) {
        NvOsDebugPrintf("%s: Received a Null parameter\n", __func__);
        return NvError_BadParameter;
    }
    pDriverProfile = &pDriver->Profile;

    hImager = hModule->hImager;
    pDesiredSettings = &(pConfigState->InputSettings);
    DesiredFlash = pDesiredSettings->Flash;
    FlashObjectList = pDriverProfile->ObjectList;

    setLevel.timeout = DesiredFlash.SustainTime;
    setLevel.ledmask = LEDMASK_OFF;
    for(i = 0; i < 2; i++) {
        setLevel.value[i] = DesiredFlash.Source[i].CurrentLevel;
//NvOsDebugPrintf("gfitzer -- %s -- seeing a non zero currentlevel[%d] of %f\n", __func__, i, setLevel.value[i]);
        if(!NVODM_F32_NEAR_EQUAL(FlashObjectList[i].CurrentLevel, DesiredFlash.Source[i].CurrentLevel)) {
            if(DesiredFlash.Source[i].CurrentLevel > 0) {
                setLevel.ledmask |=  LEDMASK_ENABLE(i);
            }
        }
    }

    if(DesiredFlash.SustainTime == PCL_FLASH_TORCH_MODE) {
        result = NvOdmImagerSetParameter(hImager, NvOdmImagerParameter_TorchLevel, sizeof(NvOdmImagerFlashSetLevel), &setLevel);
        if(!result) {
            NvOsDebugPrintf("%s: NvOdm driver failed to set flash torch level\n", __func__);
            status = NvError_NotSupported;
        }
    } else {
        result = NvOdmImagerSetParameter(hImager, NvOdmImagerParameter_FlashLevel, sizeof(NvOdmImagerFlashSetLevel), &setLevel);
        if(!result) {
            NvOsDebugPrintf("%s: NvOdm driver failed to set flash level(%d)\n", __func__);
            status = NvError_NotSupported;
        }
    }

    return status;
}


NvError NvPclNvOdmQueryStateFlash(NvPclDriver *pDriver, NvPclModuleSetting *pOutputSettings) {
    if((!pDriver) || (!pOutputSettings)) {
        NvOsDebugPrintf("%s: Received a Null parameter\n", __func__);
        return NvError_BadParameter;
    }

    return NvSuccess;
}

NvError NvPclDriverInitializeDataNvOdmSensor(NvPclCameraModuleHandle hModule, NvPclDriver *pSensorDriver) {
    NvOdmImagerHandle hImager;
    struct nvc_fuseid sensorFuseID;
    NvOdmImagerCapabilities SensorCaps;
    NvOdmImagerSensorMode *pNvOdmSensorModes = NULL;
    NvOdmImagerStaticProperty NvOdmStaticProperty;
    NvPclSensorObject *pSensorModes = NULL;
    NvS32 numModes;
    NvBool result;
    NvF32 AnalogGainRange[2];

    if((!hModule) || (!pSensorDriver)) {
        NvOsDebugPrintf("%s: Passed in Null parameters\n", __func__);
        return NvError_BadParameter;
    }

    hImager = hModule->hImager;

    // NvOdmImagerGetCapabilities is only supported by old sensor drivers
    NvOdmImagerGetCapabilities(hImager, &SensorCaps);
    NvOdmImagerListSensorModes(hImager, NULL, &numModes);
    if(numModes <= 0) {
        NvOsDebugPrintf("%s: Imager was given an invalid number of sensor modes (%s)\n", __func__, numModes);
        return NvError_InvalidState;
    }
    // Modes to query old NvOdm driver
    pNvOdmSensorModes = NvOsAlloc(numModes*sizeof(NvOdmImagerSensorMode));
    if(!pNvOdmSensorModes) {
        NvOsDebugPrintf("%s: NvOsAlloc failure on nvodm sensor modes\n", __func__);
        goto odmsensorinit_fail;
    }
    NvOsMemset(pNvOdmSensorModes, 0, numModes*sizeof(NvOdmImagerSensorMode));
    NvOdmImagerListSensorModes(hImager, pNvOdmSensorModes, &numModes);

    result = NvOdmImagerGetParameter(hImager, NvOdmImagerParameter_SensorGainLimits,
                sizeof(AnalogGainRange), AnalogGainRange);
    if(!result) {
        NvOsDebugPrintf("%s: NvOdm driver failed to read gain limits\n", __func__);
    }

    // Modes to fill in new NvPcl mode structs
    pSensorModes = NvOsAlloc(numModes*sizeof(NvPclSensorObject));
    if(!pSensorModes) {
        NvOsDebugPrintf("%s: NvOsAlloc failure on NvPcl sensor modes\n", __func__);
        goto odmsensorinit_fail;
    }
    NvOsMemset(pSensorModes, 0, numModes*sizeof(NvPclSensorObject));

    NvOsMemset(&NvOdmStaticProperty, 0, sizeof(NvOdmStaticProperty));
    result = NvOdmImagerGetStaticProperty(hImager, 0, &NvOdmStaticProperty);
    if(result != NV_TRUE) {
        NvOsDebugPrintf("%s: failed to get static properties\n", __func__);
        goto odmsensorinit_fail;
    }

    hModule->Direction = SensorCaps.Direction ?
                        NvPclModulePlacement_Rear : NvPclModulePlacement_Front;

    NvS32 m = 0;
    for(m = 0; m < numModes; m++) {
        pSensorModes[m].Id = m;
        pSensorModes[m].Version = PCL_NVODM_OBJ_VERSION;

        pSensorModes[m].PixelInfo.SensorPhysicalSize = NvOdmStaticProperty.SensorPhysicalSize;
        pSensorModes[m].PixelInfo.SensorPixelArraySize = NvOdmStaticProperty.SensorPixelArraySize;
        pSensorModes[m].PixelInfo.SensorActiveArraySize = NvOdmStaticProperty.SensorActiveArraySize;

        // Not memcopying for potential formatting change and explicity for cleanup later; note possible bugs here
        pSensorModes[m].PixelInfo.PixelType = NvOdmStaticProperty.SensorColorFilterArrangement;
        pSensorModes[m].PixelInfo.Orientation = NvOdmStaticProperty.SensorOrientation;
        pSensorModes[m].PixelInfo.SensorWhiteLevel = NvOdmStaticProperty.SensorWhiteLevel;
        pSensorModes[m].PixelInfo.BlackLevelPattern[0][0] = NvOdmStaticProperty.BlackLevelPattern[0][0];
        pSensorModes[m].PixelInfo.BlackLevelPattern[0][1] = NvOdmStaticProperty.BlackLevelPattern[0][1];
        pSensorModes[m].PixelInfo.BlackLevelPattern[1][0] = NvOdmStaticProperty.BlackLevelPattern[1][0];
        pSensorModes[m].PixelInfo.BlackLevelPattern[1][1] = NvOdmStaticProperty.BlackLevelPattern[1][1];
        pSensorModes[m].PixelInfo.ActiveDimensions = pNvOdmSensorModes[m].ActiveDimensions;
        pSensorModes[m].PixelInfo.ActiveStart = pNvOdmSensorModes[m].ActiveStart;
        pSensorModes[m].PixelInfo.MinimumBlankTime = SensorCaps.MinimumBlankTime;
        pSensorModes[m].PixelInfo.PixelAspectRatio = pNvOdmSensorModes[m].PixelAspectRatio;
        pSensorModes[m].PixelInfo.CropMode = pNvOdmSensorModes[m].CropMode;
        pSensorModes[m].PixelInfo.ModeInfo.rect = pNvOdmSensorModes[m].ModeInfo.rect;
        pSensorModes[m].PixelInfo.ModeInfo.scale = pNvOdmSensorModes[m].ModeInfo.scale;

        // Safe default values, but real values are queried when a mode has been set
        // SensorMaxFrameDuration was in nanoseconds
        pSensorModes[m].Limits.MinFrameRate =
            (NvF32)(1000000000.0f / NvOdmStaticProperty.SensorMaxFrameDuration);
        pSensorModes[m].Limits.MaxFrameRate = pNvOdmSensorModes[m].PeakFrameRate;
        pSensorModes[m].Limits.MinExposureTime = NvOdmStaticProperty.SensorExposureTimeRange[0];
        pSensorModes[m].Limits.MaxExposureTime = NvOdmStaticProperty.SensorExposureTimeRange[1];
        pSensorModes[m].Limits.MinAnalogGain = AnalogGainRange[0];
        pSensorModes[m].Limits.MaxAnalogGain = AnalogGainRange[1];
        pSensorModes[m].Limits.MinDigitalGain = 1.0;
        pSensorModes[m].Limits.MaxDigitalGain = 1.0;
        pSensorModes[m].Limits.MinHDRRatio = 1.0f;
        pSensorModes[m].Limits.MaxHDRRatio = 128.0f;

        pSensorModes[m].Interface.ClockProfile.ExternalClockKHz = SensorCaps.ClockProfiles[0].ExternalClockKHz;
        pSensorModes[m].Interface.ClockProfile.ClockMultiplier = SensorCaps.ClockProfiles[0].ClockMultiplier;

        pSensorModes[m].Interface.SensorOdmInterface = SensorCaps.SensorOdmInterface;
        pSensorModes[m].Interface.InitialSensorClockRateKHz = SensorCaps.InitialSensorClockRateKHz;
        pSensorModes[m].Interface.PLL_Multiplier = pNvOdmSensorModes[m].PLL_Multiplier;
        pSensorModes[m].Interface.ParallelTiming.HSyncEdge = SensorCaps.ParallelTiming.HSyncEdge;
        pSensorModes[m].Interface.ParallelTiming.VSyncEdge = SensorCaps.ParallelTiming.VSyncEdge;
        pSensorModes[m].Interface.ParallelTiming.MClkOnVGP0 = SensorCaps.ParallelTiming.MClkOnVGP0;
        pSensorModes[m].Interface.MipiTiming.CsiPort = SensorCaps.MipiTiming.CsiPort;
        pSensorModes[m].Interface.MipiTiming.DataLanes = SensorCaps.MipiTiming.DataLanes;
        pSensorModes[m].Interface.MipiTiming.VirtualChannelID = SensorCaps.MipiTiming.VirtualChannelID;
        pSensorModes[m].Interface.MipiTiming.DiscontinuousClockMode = SensorCaps.MipiTiming.DiscontinuousClockMode;
        pSensorModes[m].Interface.MipiTiming.CILThresholdSettle = SensorCaps.MipiTiming.CILThresholdSettle;

    }

    pSensorDriver->Profile.Type = NvPclDriverType_Sensor;
    pSensorDriver->Profile.NumObjects = numModes;
    pSensorDriver->Profile.ObjectList = pSensorModes;
    pSensorDriver->Profile.pActiveObject = pSensorModes;

    result = NvOdmImagerGetParameter(hImager, NvOdmImagerParameter_FuseID,
        sizeof(struct nvc_fuseid), &sensorFuseID);
    if(!result) {
        NvOsDebugPrintf("%s: NvOdm driver did not provide sensor FuseID\n", __func__);
    } else {
        if(sensorFuseID.size > PCL_MAX_SIZE_FUSEID){
            NvOsDebugPrintf("%s: Imager FuseID size exceeds upper limit\n", __func__);
            pSensorDriver->Profile.FuseID.NumBytes = PCL_MAX_SIZE_FUSEID;
        } else {
            pSensorDriver->Profile.FuseID.NumBytes = (NvU8)sensorFuseID.size;
        }
        NvOsMemset(pSensorDriver->Profile.FuseID.Data, 0, PCL_MAX_SIZE_FUSEID);
        NvOsMemcpy(pSensorDriver->Profile.FuseID.Data, sensorFuseID.data,
                    pSensorDriver->Profile.FuseID.NumBytes);
    }

    NvOdmImagerCalibrationData cData;
    result = NvOdmImagerGetParameter(hImager, NvOdmImagerParameter_CalibrationData, sizeof(NvOdmImagerCalibrationData), &cData);
    if(!result) {
        PCL_DB_PRINTF("%s: NvOdm driver did not provide calibration data\n", __func__);
    } else {
        hModule->CalibrationData.Size = NvOsStrlen(cData.CalibrationData)+1;
        hModule->CalibrationData.pData = NvOsRealloc(hModule->CalibrationData.pData, hModule->CalibrationData.Size);
        if(!hModule->CalibrationData.pData) {
            NvOsDebugPrintf("%s: NvOsAlloc failure on calibration data\n", __func__);
        } else {
            NvOsMemcpy(hModule->CalibrationData.pData, cData.CalibrationData, hModule->CalibrationData.Size);
        }
        if(cData.NeedsFreeing) {
            NvOsFree((void*)cData.CalibrationData);
            cData.NeedsFreeing = NV_FALSE;
        }
    }

    result = NvOdmImagerGetParameter(hImager, NvOdmImagerParameter_CalibrationOverrides, sizeof(NvOdmImagerCalibrationData), &cData);
    if(!result) {
        PCL_DB_PRINTF("%s: NvOdm driver did not provide calibration overrides\n", __func__);
    } else {
        hModule->CalibrationOverride.Size = NvOsStrlen(cData.CalibrationData)+1;
        hModule->CalibrationOverride.pData = NvOsRealloc(hModule->CalibrationOverride.pData, hModule->CalibrationOverride.Size);
        if(!hModule->CalibrationOverride.pData) {
            NvOsDebugPrintf("%s: NvOsAlloc failure on calibration override data\n", __func__);
        } else {
            NvOsMemcpy(hModule->CalibrationOverride.pData, cData.CalibrationData, hModule->CalibrationOverride.Size);
        }
        if(cData.NeedsFreeing) {
            NvOsFree((void*)cData.CalibrationData);
            cData.NeedsFreeing = NV_FALSE;
        }
    }

    // Factory calibration blobs are different...
    hModule->FactoryCalibration.Size = MAX_FLASH_SIZE;
    hModule->FactoryCalibration.pData = NvOsRealloc(hModule->FactoryCalibration.pData, hModule->FactoryCalibration.Size);
    result = NvOdmImagerGetParameter(hImager, NvOdmImagerParameter_FactoryCalibrationData,
        MAX_FLASH_SIZE, hModule->FactoryCalibration.pData);
    if(!result) {
        PCL_DB_PRINTF("%s: NvOdm driver did not provide factory calibration data\n", __func__);
    }


    NvOsFree(pNvOdmSensorModes);

    pSensorDriver->pfnPclDriverGetVolatile = NvPclNvOdmQueryStateSensor;
    pSensorDriver->pfnPclDriverClose = NvPclNvOdmCloseDriver;

    return NvSuccess;

odmsensorinit_fail:
    NvOsDebugPrintf("%s: Failed to init driver.\n", __func__);
    if(pNvOdmSensorModes) {
        NvOsFree(pNvOdmSensorModes);
    }
    if(pSensorModes) {
        NvOsFree(pSensorModes);
        pSensorModes = NULL;
    }

    return NvError_InsufficientMemory;
}

NvError NvPclDriverInitializeDataNvOdmLensStub(NvPclCameraModuleHandle hModule, NvPclDriver *pLensDriver) {
    NvOdmImagerHandle hImager;
    NvOdmImagerStaticProperty NvOdmStaticProperty;
    NvPclFocuserObject *pNvPclFocuserObject = NULL;
    NvBool result = NV_FALSE;
    NvF32 fNumber = 0;

    if((!hModule) || (!pLensDriver)) {
        NvOsDebugPrintf("%s: Passed in Null parameters\n", __func__);
        return NvError_BadParameter;
    }

    hImager = hModule->hImager;

    pNvPclFocuserObject = NvOsAlloc(sizeof(NvPclFocuserObject));
    if(!pNvPclFocuserObject) {
        NvOsDebugPrintf("%s: NvOsAlloc failure on NvPcl focuser modes\n", __func__);
        goto odmlensinit_fail;
    }
    NvOsMemset(pNvPclFocuserObject, 0, sizeof(NvPclFocuserObject));

    NvOsMemset(&NvOdmStaticProperty, 0, sizeof(NvOdmStaticProperty));
    result = NvOdmImagerGetStaticProperty(hImager, 0, &NvOdmStaticProperty);
    if(result != NV_TRUE) {
        NvOsDebugPrintf("%s: failed to read lens static properties\n", __func__);
        goto odmlensinit_fail;
    }

    result = NvOdmImagerGetParameter(hImager, NvOdmImagerParameter_FNumber, sizeof(NvF32), &fNumber);
    if(!result) {
        NvOsDebugPrintf("%s: NvOdm driver failed to read FNumber\n", __func__);
    }

    pNvPclFocuserObject->Version = PCL_NVODM_OBJ_VERSION;
    pNvPclFocuserObject->HyperFocal = NvOdmStaticProperty.LensHyperfocalDistance;
    pNvPclFocuserObject->FocalLength = NvOdmStaticProperty.LensAvailableFocalLengths.Values[0];
    pNvPclFocuserObject->MinFocusDistance = NvOdmStaticProperty.LensMinimumFocusDistance;
    pNvPclFocuserObject->FNumber = fNumber;

    /* WAR: for sensors with no focusers, max aperture is
     * hardcoded to 2.8f (a generic aperture value), this
     * ensures no garbage value is written by core.
     * Bug 1454108.
     * WAR will be removed with further PCL progresss
     * */

    pNvPclFocuserObject->MaxAperture = 2.8f;

    pLensDriver->Profile.Type = NvPclDriverType_Focuser;
    pLensDriver->Profile.NumObjects = 1;
    pLensDriver->Profile.ObjectList = pNvPclFocuserObject;
    pLensDriver->Profile.pActiveObject = pNvPclFocuserObject;

    return NvSuccess;

odmlensinit_fail:
    NvOsDebugPrintf("%s: Failed to init lens.\n", __func__);
    if(pNvPclFocuserObject) {
        NvOsFree(pNvPclFocuserObject);
        pNvPclFocuserObject = NULL;
    }

    return NvError_InsufficientMemory;
}


NvError NvPclDriverInitializeDataNvOdmFocuser(NvPclCameraModuleHandle hModule, NvPclDriver *pFocuserDriver) {
    NvOdmImagerHandle hImager;
    NvOdmImagerFocuserCapabilities FocuserCaps;
    NvOdmImagerStaticProperty NvOdmStaticProperty;
    NvPclFocuserObject *pNvPclFocuserObjects = NULL;
    NvPclFocuserContext *pContext = NULL;
    NvBool result = NV_FALSE;
    NvU32 numObjects = 1;
    NvF32 focalLength = 0;
    NvF32 maxAperture = 0;
    NvF32 fNumber = 0;
    NvU32 i = 0;

    if((!hModule) || (!pFocuserDriver)) {
        NvOsDebugPrintf("%s: Passed in Null parameters\n", __func__);
        return NvError_BadParameter;
    }

    hImager = hModule->hImager;

    pNvPclFocuserObjects = NvOsAlloc(numObjects*sizeof(NvPclFocuserObject));
    if(!pNvPclFocuserObjects) {
        NvOsDebugPrintf("%s: NvOsAlloc failure on NvPcl focuser modes\n", __func__);
        goto odmfocuserinit_fail;
    }
    NvOsMemset(pNvPclFocuserObjects, 0, numObjects*sizeof(NvPclFocuserObject));

    pContext = NvOsAlloc(sizeof(NvPclFocuserContext));
    if(!pContext) {
        NvOsDebugPrintf("%s: NvOsAlloc failure on NvPcl focuser context\n", __func__);
        goto odmfocuserinit_fail;
    }
    NvOsMemset(pContext, 0, sizeof(NvPclFocuserContext));

    NvOsMemset(&FocuserCaps, 0, sizeof(NvOdmImagerFocuserCapabilities));
    result = NvOdmImagerGetParameter(hImager, NvOdmImagerParameter_FocuserCapabilities, sizeof(NvOdmImagerFocuserCapabilities), &FocuserCaps);
    if(!result) {
        NvOsDebugPrintf("%s: NvOdm driver failed to read focuser capabilities\n", __func__);
        goto odmfocuserinit_fail;
    }

    result = NvOdmImagerGetParameter(hImager, NvOdmImagerParameter_FocalLength, sizeof(NvF32), &focalLength);
    if(!result) {
        NvOsDebugPrintf("%s: NvOdm driver failed to read focuser focal length\n", __func__);
        goto odmfocuserinit_fail;
    }

    result = NvOdmImagerGetParameter(hImager, NvOdmImagerParameter_MaxAperture, sizeof(NvF32), &maxAperture);
    if(!result) {
        NvOsDebugPrintf("%s: NvOdm driver failed to read focuser max aperture\n", __func__);
        goto odmfocuserinit_fail;
    }

    result = NvOdmImagerGetParameter(hImager, NvOdmImagerParameter_FNumber, sizeof(NvF32), &fNumber);
    if(!result) {
        NvOsDebugPrintf("%s: NvOdm driver failed to read focuser FNumber\n", __func__);
        goto odmfocuserinit_fail;
    }

    NvOsMemset(&NvOdmStaticProperty, 0, sizeof(NvOdmStaticProperty));
    result = NvOdmImagerGetStaticProperty(hImager, 0, &NvOdmStaticProperty);
    if(result != NV_TRUE) {
        NvOsDebugPrintf("%s: failed to read focuser static properties\n", __func__);
        goto odmfocuserinit_fail;
    }

    for(i = 0; i < numObjects; i++) {
        pNvPclFocuserObjects[i].Version = PCL_NVODM_OBJ_VERSION;
        pNvPclFocuserObjects[i].PositionMin = FocuserCaps.positionActualLow;
        pNvPclFocuserObjects[i].PositionMax = FocuserCaps.positionActualHigh;

        pNvPclFocuserObjects[i].HyperFocal = NvOdmStaticProperty.LensHyperfocalDistance;
        pNvPclFocuserObjects[i].FocalLength = NvOdmStaticProperty.LensAvailableFocalLengths.Values[0];
        pNvPclFocuserObjects[i].MaxAperture = maxAperture;
        pNvPclFocuserObjects[i].FNumber = fNumber;
        pNvPclFocuserObjects[i].MinFocusDistance =
                NvOdmStaticProperty.LensMinimumFocusDistance;
    }

    pFocuserDriver->pPrivateContext = pContext;
    pFocuserDriver->Profile.Type = NvPclDriverType_Focuser;
    pFocuserDriver->Profile.NumObjects = numObjects;
    pFocuserDriver->Profile.ObjectList = pNvPclFocuserObjects;
    pFocuserDriver->Profile.pActiveObject = pNvPclFocuserObjects;

    pFocuserDriver->pfnPclDriverGetVolatile = NvPclNvOdmQueryStateFocuser;
    pFocuserDriver->pfnPclDriverClose = NvPclNvOdmCloseDriver;

    return NvSuccess;

odmfocuserinit_fail:
    NvOsDebugPrintf("%s: Failed to init driver.\n", __func__);
    if(pNvPclFocuserObjects) {
        NvOsFree(pNvPclFocuserObjects);
        pNvPclFocuserObjects = NULL;
    }
    if(pContext) {
        NvOsFree(pContext);
        pContext = NULL;
    }

    return NvError_InsufficientMemory;
}



NvError NvPclDriverInitializeDataNvOdmFlash(NvPclCameraModuleHandle hModule, NvPclDriver *pFlashDriver) {
    NvOdmImagerHandle hImager;
    NvPclFlashObject *pNvPclFlashObjects = NULL;
    NvOdmImagerStaticProperty NvOdmStaticProperty;
    NvOdmImagerFlashCapabilities flashCaps;
    NvOdmImagerTorchCapabilities torchCaps;
    NvOdmImagerFlashTorchQuery query;
    NvBool result = NV_FALSE;
    NvU32 numObjects = 0;
    NvU32 i = 0;
    NvU32 j = 0;

    if((!hModule) || (!pFlashDriver)) {
        NvOsDebugPrintf("%s: Passed in Null parameters\n", __func__);
        return NvError_BadParameter;
    }

    hImager = hModule->hImager;

    NvOsMemset(&NvOdmStaticProperty, 0, sizeof(NvOdmStaticProperty));
    result = NvOdmImagerGetStaticProperty(hImager, 0, &NvOdmStaticProperty);
    if(result != NV_TRUE) {
        NvOsDebugPrintf("%s: failed to read flash static properties\n", __func__);
        goto odmflashinit_fail;
    }
    numObjects = NvOdmStaticProperty.AvailableLeds.Size;

    pNvPclFlashObjects = NvOsAlloc(numObjects*sizeof(NvPclFlashObject));
    if(!pNvPclFlashObjects) {
        NvOsDebugPrintf("%s: NvOsAlloc failure on NvPcl flash modes\n", __func__);
        goto odmflashinit_fail;
    }
    NvOsMemset(pNvPclFlashObjects, 0, numObjects*sizeof(NvPclFlashObject));

    NvOsMemset(&query, 0, sizeof(NvOdmImagerFlashTorchQuery));
    result = NvOdmImagerGetParameter(hImager, NvOdmImagerParameter_FlashTorchQuery, sizeof(NvOdmImagerFlashTorchQuery), &query);
    if(!result) {
        NvOsDebugPrintf("%s: NvOdm driver failed to read flash query\n", __func__);
        goto odmflashinit_fail;
    }

    NvOsMemset(&flashCaps, 0, sizeof(NvOdmImagerFlashCapabilities));
    result = NvOdmImagerGetParameter(hImager, NvOdmImagerParameter_FlashCapabilities, sizeof(NvOdmImagerFlashCapabilities), &flashCaps);
    if(!result) {
        NvOsDebugPrintf("%s: NvOdm driver failed to read flash capabilities\n", __func__);
        goto odmflashinit_fail;
    }

    NvOsMemset(&torchCaps, 0, sizeof(NvOdmImagerTorchCapabilities));
    result = NvOdmImagerGetParameter(hImager, NvOdmImagerParameter_TorchCapabilities, sizeof(NvOdmImagerTorchCapabilities), &torchCaps);
    if(!result) {
        NvOsDebugPrintf("%s: NvOdm driver failed to read flash query\n", __func__);
        goto odmflashinit_fail;
    }

    for(i = 0; i < numObjects; i++) {
        pNvPclFlashObjects[i].Version = PCL_NVODM_OBJ_VERSION;
        pNvPclFlashObjects[i].Id = i;
        pNvPclFlashObjects[i].ColorTemperature.x = 0;
        pNvPclFlashObjects[i].ColorTemperature.y = 0;
        pNvPclFlashObjects[i].CurrentLevel = 0;
        pNvPclFlashObjects[i].FlashChargeDuration = 0;

        pNvPclFlashObjects[i].NumFlashCurrentLevels = flashCaps.NumberOfLevels;
        for(j = 0; j < flashCaps.NumberOfLevels; j++) {
            pNvPclFlashObjects[i].FlashCurrentLevel[j] = flashCaps.levels[j].guideNum;
        }

        pNvPclFlashObjects[i].NumTorchCurrentLevels = torchCaps.NumberOfLevels;
        NvOsMemcpy(pNvPclFlashObjects[i].TorchCurrentLevel,
                    torchCaps.guideNum, sizeof(NvF32)*torchCaps.NumberOfLevels);
    }

    pFlashDriver->Profile.Type = NvPclDriverType_Flash;
    pFlashDriver->Profile.NumObjects = numObjects;
    pFlashDriver->Profile.ObjectList = pNvPclFlashObjects;
    pFlashDriver->Profile.pActiveObject = pNvPclFlashObjects;

    pFlashDriver->pfnPclDriverGetVolatile = NvPclNvOdmQueryStateFlash;
    pFlashDriver->pfnPclDriverClose = NvPclNvOdmCloseDriver;

    return NvSuccess;

odmflashinit_fail:
    NvOsDebugPrintf("%s: Failed to init driver.\n", __func__);
    if(pNvPclFlashObjects) {
        NvOsFree(pNvPclFlashObjects);
        pNvPclFlashObjects = NULL;
    }

    return NvError_InsufficientMemory;
}

void NvPclNvOdmCloseDriver(NvPclDriver *pDriver) {
    if(!pDriver) {
        NvOsDebugPrintf("%s: Passed in Null parameters\n", __func__);
        return;
    }

    if(pDriver->Profile.ObjectList) {
        NvOsFree(pDriver->Profile.ObjectList);
        pDriver->Profile.ObjectList = NULL;
    }
    if(pDriver->pPrivateContext) {
        NvOsFree(pDriver->pPrivateContext);
        pDriver->pPrivateContext = NULL;
    }

    return;
}

NvError NvPclNvOdmApplyState(NvPclControllerHandle hPclController) {
    StateControllerContext *pContext;
    NvPclCameraModule *pModule;
    NvU8 i = 0;
    NvError (*pfnWriteState)(NvPclCameraModuleHandle hModule,
                            NvPclDriver *pDriver,
                            NvPclControlState *pConfigState);

    if((!hPclController) || (!hPclController->pPrivateContext)) {
        NvOsDebugPrintf("%s: Passed uninitialized NvPclControllerHandle\n", __func__);
        return NvError_BadParameter;
    }

    pContext = hPclController->pPrivateContext;
    pModule = hPclController->PlatformData.hFirstActiveModule;

    /**
     * Consider if we actually want to use failures/errors during POC legacy transition
     */
    // Do hImager iterative calls
    for(i = 0; i < pModule->NumSubDrivers; i++) {
        if(pModule->SubDriver[i].Profile.IsOldNvOdmDriver) {
            pfnWriteState = NULL;
            switch(pModule->SubDriver[i].Profile.Type) {
                case NvPclDriverType_Sensor:
                    pfnWriteState = NvPclNvOdmWriteStateSensor;
                    break;
                case NvPclDriverType_Focuser:
                    pfnWriteState = NvPclNvOdmWriteStateFocuser;
                    break;
                case NvPclDriverType_Flash:
                    pfnWriteState = NvPclNvOdmWriteStateFlash;
                    break;
                default:
                    NvOsDebugPrintf("%s: Unrecognized %s isOldNvOdmDriver type\n",
                        __func__, pModule->SubDriver[i].Profile.HwDev.Name);
                    return NvError_InvalidState;
            }
            if(pfnWriteState) {
                pfnWriteState(pModule, &pModule->SubDriver[i],
                            &pContext->ConfigurationState);
            }
        }
    }

    return NvSuccess;
}

