/*
 * Copyright (c) 2007-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#include "nvcommon.h"
#include "nvassert.h"
#include "nvddk_disp_hw.h"
#include "dc_hal.h"
#include "dc_sd3.h"
#include "dc_crt_hal.h"
#include "dc_hdmi_hal.h"
#include "dc_dsi_hal.h"

NvDdkDispBuiltin dc_hdmi_builtin =
    {
        DcHdmiEnable,
        DcHdmiListModes,
        DcHdmiSetPowerLevel,
        DcHdmiGetParameter,
        DcHdmiGetGuid,
        DcHdmiSetEdid,
        DcHdmiAudioFrequency,
    };

NvDdkDispBuiltin dc_crt_builtin =
    {
        DcCrtEnable,
        DcCrtListModes,
        DcCrtSetPowerLevel,
        DcCrtGetParameter,
        DcCrtGetGuid,
        DcCrtSetEdid,
        0, // no audio
    };

NvDdkDispHdmiHal dc_hdmi_hal =
    {
        DcHdcpOp,
    };

NvOdmDispDsiTransport dc_dsi_transport =
    {
        DcDsiTransInit,
        DcDsiTransDeinit,
        DcDsiTransWrite,
        DcDsiTransRead,
        DcDsiTransEnableCommandMode,
        DcDsiTransUpdate,
        DcDsiTransGetPhyFreq,
        DcDsiTransPing,
    };

static NvBool discover( NvOdmIoModule mod )
{
    NvU64 guid;
    NvU32 vals[] =
        {
            NvOdmPeripheralClass_Display,
            0,
        };
    NvOdmPeripheralSearch search[] =
        {
            NvOdmPeripheralSearch_PeripheralClass,
            NvOdmPeripheralSearch_IoModule,
        };

    vals[1] = mod;

    /* find a display guid */
    if( !NvOdmPeripheralEnumerate( search, vals, 2, &guid, 1 ) )
    {
        return NV_FALSE;
    }

    return NV_TRUE;
}

void
NvDdkDispHalInit( NvDdkDispCapabilities *caps, NvDdkDispHal *hal )
{
    NV_ASSERT( caps );
    NV_ASSERT( hal );

    /* ap10 support has been removed */
    NV_ASSERT( caps->engine != NvDdkDispEngine_Dvo );

    if( caps->engine == NvDdkDispEngine_Dc )
    {
        hal->HwOpen = DcHal_HwOpen;
        hal->HwClose = DcHal_HwClose;
        hal->HwSuspend = DcHal_HwSuspend;
        hal->HwResume = DcHal_HwResume;
        hal->HwBegin = DcHal_HwBegin;
        hal->HwEnd = DcHal_HwEnd;
        hal->HwTriggerFrame = DcHal_HwTriggerFrame;
        hal->HwCacheClear = DcHal_HwCacheClear;
        hal->HwSetMode = DcHal_HwSetMode;
        hal->HwSetBaseColorSize = DcHal_HwSetBaseColorSize;
        hal->HwSetDataAlignment = DcHal_HwSetDataAlignment;
        hal->HwSetPinMap = DcHal_HwSetPinMap;
        hal->HwSetPwmOutputPin = DcHal_HwSetPwmOutputPin;
        hal->HwSetPinPolarities = DcHal_HwSetPinPolarities;
        hal->HwSetPeriodicInstructions = DcHal_HwSetPeriodicInstructions;
        hal->HwSetWindowSurface = DcHal_HwSetWindowSurface;
        hal->HwFlipWindowSurface = DcHal_HwFlipWindowSurface;
        hal->HwSetDepth = DcHal_HwSetDepth;
        hal->HwSetFiltering = DcHal_HwSetFiltering;
        hal->HwSetVibrance = DcHal_HwSetVibrance;
        hal->HwSetScaleNicely = DcHal_HwSetScaleNicely;
        hal->HwSetBlend = DcHal_HwSetBlend;
        hal->HwSetAlphaOp = DcHal_HwSetAlphaOp;
        hal->HwSetAlpha = DcHal_HwSetAlpha;
        hal->HwSetCsc = DcHal_HwSetCsc;
        hal->HwSetColorKey = DcHal_HwSetColorKey;
        hal->HwSetColorCorrection = DcHal_HwSetColorCorrection;
        hal->HwSetBacklight = DcHal_HwSetBacklight;
        hal->HwSetBackgroundColor = DcHal_HwSetBackgroundColor;
        hal->HwSetCursor = DcHal_HwSetCursor;
        hal->HwSetCursorPosition = DcHal_HwSetCursorPosition;
        hal->HwContentProtection = DcHal_HwContentProtection;
        hal->HwSetErrorTrigger = DcHal_SetErrorTrigger;
        hal->HwGetLastActiveError = DcHal_HwGetLastActiveError;
        hal->HwSetErrorSem = DcHal_HwSetErrorSem;
        hal->HwGetLastError = DcHal_HwGetLastError;
        hal->HwSmartDimmerConfig = DcHal_HwSmartDimmerConfig;
        hal->HwSmartDimmerBrightness = DcHal_HwSmartDimmerBrightness;
        hal->HwSmartDimmerLog = DcHal_HwSmartDimmerLog;
        hal->HwDsiEnable = DcHal_HwDsiEnable;
        hal->HwDsiSetIndex = DcHal_HwDsiSetIndex;
        hal->HwDsiCommandModeUpdate = DcHal_HwDsiCommandModeUpdate;
        hal->HwDsiSwitchMode = DcHal_HwDsiSwitchMode;

        hal->HwTestCrcEnable = DcHal_HwTestCrcEnable;
        hal->HwTestCrc = DcHal_HwTestCrc;
        hal->HwTestInjectError = DcHal_HwTestInjectError;

        hal->Crt = 0;
        if( discover( NvOdmIoModule_Crt ) )
        {
            hal->Crt = &dc_crt_builtin;
        }

        hal->Hdmi = 0;
        hal->HdmiHal = 0;
        if( discover( NvOdmIoModule_Hdmi ) )
        {
            hal->Hdmi = &dc_hdmi_builtin;
            hal->HdmiHal = &dc_hdmi_hal;
        }

        hal->Tv = 0;
        hal->TvHal = 0;

        hal->DsiTransport = &dc_dsi_transport;
    }
}
