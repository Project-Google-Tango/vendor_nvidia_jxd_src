/*
 * Copyright (c) 2011-2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvimage_enc_makernote_extension_serializer.h"

NvError NvCameraInsertToJDS(NvCameraJDS *db, NvCameraMakerNoteExtension *nvCameraMakerNoteExtension) {
    NvError e;

    e = NvCameraJDSInsertNvS32Array(db, "ToneCurve.v2.data.UserSpaceCurves",
        &nvCameraMakerNoteExtension->ToneCurve.v2.data.UserSpaceCurves[0],
        sizeof(nvCameraMakerNoteExtension->ToneCurve.v2.data.UserSpaceCurves));
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvS32Array(db, "ToneCurve.v2.data.UserAdvanceSpaceCurves",
        &nvCameraMakerNoteExtension->ToneCurve.v2.data.UserAdvanceSpaceCurves[0],
        sizeof(nvCameraMakerNoteExtension->ToneCurve.v2.data.UserAdvanceSpaceCurves));
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32(db, "ToneCurve.v2.data.HistogramWidth",
         nvCameraMakerNoteExtension->ToneCurve.v2.data.HistogramWidth);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32(db, "ToneCurve.v2.data.Brightness",
         nvCameraMakerNoteExtension->ToneCurve.v2.data.Brightness);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvS32Array(db, "ToneCurve.v2.data.NvidiaSpaceCurves",
        &nvCameraMakerNoteExtension->ToneCurve.v2.data.NvidiaSpaceCurves[0],
        sizeof(nvCameraMakerNoteExtension->ToneCurve.v2.data.NvidiaSpaceCurves));
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32(db, "ToneCurve.v2.data.Median",
         nvCameraMakerNoteExtension->ToneCurve.v2.data.Median);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvS32Array(db, "ToneCurve.v2.data.Results",
        &nvCameraMakerNoteExtension->ToneCurve.v2.data.Results[0],
        sizeof(nvCameraMakerNoteExtension->ToneCurve.v2.data.Results));
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32(db, "ToneCurve.v2.data.BlackPercentile",
         nvCameraMakerNoteExtension->ToneCurve.v2.data.BlackPercentile);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32Array(db, "ToneCurve.v2.data.P",
        &nvCameraMakerNoteExtension->ToneCurve.v2.data.P[0],
        sizeof(nvCameraMakerNoteExtension->ToneCurve.v2.data.P));
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32(db, "ToneCurve.v2.data.Flare1",
         nvCameraMakerNoteExtension->ToneCurve.v2.data.Flare1);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32(db, "ToneCurve.v2.data.Gain",
         nvCameraMakerNoteExtension->ToneCurve.v2.data.Gain);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32(db, "ToneCurve.v2.data.dx",
         nvCameraMakerNoteExtension->ToneCurve.v2.data.dx);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32(db, "ToneCurve.v2.data.dy",
         nvCameraMakerNoteExtension->ToneCurve.v2.data.dy);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32(db, "ToneCurve.v2.data.alpha",
         nvCameraMakerNoteExtension->ToneCurve.v2.data.alpha);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32(db, "ToneCurve.v2.data.FlarePercentile",
         nvCameraMakerNoteExtension->ToneCurve.v2.data.FlarePercentile);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32(db, "ToneCurve.v2.data.sigma",
         nvCameraMakerNoteExtension->ToneCurve.v2.data.sigma);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32(db, "ToneCurve.v2.data.DarkLevel2",
         nvCameraMakerNoteExtension->ToneCurve.v2.data.DarkLevel2);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32(db, "UserSettings.Hue",
         nvCameraMakerNoteExtension->UserSettings.Hue);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32(db, "UserSettings.Saturation",
         nvCameraMakerNoteExtension->UserSettings.Saturation);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32(db, "UserSettings.Brightness",
         nvCameraMakerNoteExtension->UserSettings.Brightness);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32(db, "UserSettings.Contrast",
         nvCameraMakerNoteExtension->UserSettings.Contrast);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32(db, "UserSettings.Gamma",
         nvCameraMakerNoteExtension->UserSettings.Gamma);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvBool(db, "AutoWhiteBalance.HalfPress",
         nvCameraMakerNoteExtension->AutoWhiteBalance.HalfPress);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32(db, "AutoWhiteBalance.Tint",
         nvCameraMakerNoteExtension->AutoWhiteBalance.Tint);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32(db, "AutoWhiteBalance.WorstFitSamples",
         nvCameraMakerNoteExtension->AutoWhiteBalance.WorstFitSamples);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32(db, "AutoWhiteBalance.LUX",
         nvCameraMakerNoteExtension->AutoWhiteBalance.LUX);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvS32(db, "AutoWhiteBalance.CCT",
         nvCameraMakerNoteExtension->AutoWhiteBalance.CCT);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32Array(db, "AutoWhiteBalance.Cest",
        &nvCameraMakerNoteExtension->AutoWhiteBalance.Cest[0],
        sizeof(nvCameraMakerNoteExtension->AutoWhiteBalance.Cest));
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32Array(db, "AutoWhiteBalance.GrayPointEst1",
        &nvCameraMakerNoteExtension->AutoWhiteBalance.GrayPointEst1[0],
        sizeof(nvCameraMakerNoteExtension->AutoWhiteBalance.GrayPointEst1));
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32Array(db, "AutoWhiteBalance.GrayPointEst2",
        &nvCameraMakerNoteExtension->AutoWhiteBalance.GrayPointEst2[0],
        sizeof(nvCameraMakerNoteExtension->AutoWhiteBalance.GrayPointEst2));
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32Array(db, "AutoWhiteBalance.GrayPointEst3",
        &nvCameraMakerNoteExtension->AutoWhiteBalance.GrayPointEst3[0],
        sizeof(nvCameraMakerNoteExtension->AutoWhiteBalance.GrayPointEst3));
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvS32Array(db, "AutoWhiteBalance.CCTRange",
        &nvCameraMakerNoteExtension->AutoWhiteBalance.CCTRange[0],
        sizeof(nvCameraMakerNoteExtension->AutoWhiteBalance.CCTRange));
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertcharArray(db, "AutoWhiteBalance.AlgorithmStatus",
        &nvCameraMakerNoteExtension->AutoWhiteBalance.AlgorithmStatus[0],
        sizeof(nvCameraMakerNoteExtension->AutoWhiteBalance.AlgorithmStatus));
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32(db, "AutoWhiteBalance.GrayPointEst2_X",
         nvCameraMakerNoteExtension->AutoWhiteBalance.GrayPointEst2_X);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32(db, "AutoWhiteBalance.BestFitSamples",
         nvCameraMakerNoteExtension->AutoWhiteBalance.BestFitSamples);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32(db, "AutoWhiteBalance.GrayPointEst3_Y",
         nvCameraMakerNoteExtension->AutoWhiteBalance.GrayPointEst3_Y);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32Array(db, "AutoWhiteBalance.GrayLineThickness",
        &nvCameraMakerNoteExtension->AutoWhiteBalance.GrayLineThickness[0],
        sizeof(nvCameraMakerNoteExtension->AutoWhiteBalance.GrayLineThickness));
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvBool(db, "AutoWhiteBalance.GrassAvoidEnable",
         nvCameraMakerNoteExtension->AutoWhiteBalance.GrassAvoidEnable);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32(db, "AutoWhiteBalance.ExpectedFitError",
         nvCameraMakerNoteExtension->AutoWhiteBalance.ExpectedFitError);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32Array(db, "AutoWhiteBalance.uv1",
        &nvCameraMakerNoteExtension->AutoWhiteBalance.uv1[0],
        sizeof(nvCameraMakerNoteExtension->AutoWhiteBalance.uv1));
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32Array(db, "AutoWhiteBalance.uv2",
        &nvCameraMakerNoteExtension->AutoWhiteBalance.uv2[0],
        sizeof(nvCameraMakerNoteExtension->AutoWhiteBalance.uv2));
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvS32Array(db, "AutoWhiteBalance.Timer",
        &nvCameraMakerNoteExtension->AutoWhiteBalance.Timer[0],
        sizeof(nvCameraMakerNoteExtension->AutoWhiteBalance.Timer));
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32Array(db, "AutoWhiteBalance.GLest",
        &nvCameraMakerNoteExtension->AutoWhiteBalance.GLest[0],
        sizeof(nvCameraMakerNoteExtension->AutoWhiteBalance.GLest));
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32Array(db, "AutoWhiteBalance.UV2_XY",
        &nvCameraMakerNoteExtension->AutoWhiteBalance.UV2_XY[0],
        sizeof(nvCameraMakerNoteExtension->AutoWhiteBalance.UV2_XY));
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvS32(db, "AutoWhiteBalance.Sample",
         nvCameraMakerNoteExtension->AutoWhiteBalance.Sample);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32Array(db, "AutoWhiteBalance.AWBGains",
        &nvCameraMakerNoteExtension->AutoWhiteBalance.AWBGains[0],
        sizeof(nvCameraMakerNoteExtension->AutoWhiteBalance.AWBGains));
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvBool(db, "AutoExposure.HalfPress",
         nvCameraMakerNoteExtension->AutoExposure.HalfPress);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertcharArray(db, "AutoExposure.AlgorithmStatus",
        &nvCameraMakerNoteExtension->AutoExposure.AlgorithmStatus[0],
        sizeof(nvCameraMakerNoteExtension->AutoExposure.AlgorithmStatus));
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32(db, "AutoExposure.AE.ExposureTime",
         nvCameraMakerNoteExtension->AutoExposure.AE.ExposureTime);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32(db, "AutoExposure.AE.SensorGain",
         nvCameraMakerNoteExtension->AutoExposure.AE.SensorGain);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32(db, "AutoExposure.AE.Gain",
         nvCameraMakerNoteExtension->AutoExposure.AE.Gain);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32Array(db, "AutoExposure.Limits.ExposureTime",
        &nvCameraMakerNoteExtension->AutoExposure.Limits.ExposureTime[0],
        sizeof(nvCameraMakerNoteExtension->AutoExposure.Limits.ExposureTime));
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32Array(db, "AutoExposure.Limits.FrameRate",
        &nvCameraMakerNoteExtension->AutoExposure.Limits.FrameRate[0],
        sizeof(nvCameraMakerNoteExtension->AutoExposure.Limits.FrameRate));
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32Array(db, "AutoExposure.Limits.Gain",
        &nvCameraMakerNoteExtension->AutoExposure.Limits.Gain[0],
        sizeof(nvCameraMakerNoteExtension->AutoExposure.Limits.Gain));
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32(db, "AutoExposure.CommonGain.CommonGain",
         nvCameraMakerNoteExtension->AutoExposure.CommonGain.CommonGain);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32(db, "AutoExposure.CommonGain.Brightness",
         nvCameraMakerNoteExtension->AutoExposure.CommonGain.Brightness);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvS32Array(db, "AutoExposure.Timer",
        &nvCameraMakerNoteExtension->AutoExposure.Timer[0],
        sizeof(nvCameraMakerNoteExtension->AutoExposure.Timer));
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32(db, "AutoExposure.Luma.Measured",
         nvCameraMakerNoteExtension->AutoExposure.Luma.Measured);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32(db, "AutoExposure.Luma.Target",
         nvCameraMakerNoteExtension->AutoExposure.Luma.Target);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32Array(db, "ColorCorrection.ColorCorrection",
        &nvCameraMakerNoteExtension->ColorCorrection.ColorCorrection[0][0],
        sizeof(nvCameraMakerNoteExtension->ColorCorrection.ColorCorrection));
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32Array(db, "ColorCorrection.RealColorCorrection",
        &nvCameraMakerNoteExtension->ColorCorrection.RealColorCorrection[0][0],
        sizeof(nvCameraMakerNoteExtension->ColorCorrection.RealColorCorrection));
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvS32(db, "LensShading.shapes.cct2",
         nvCameraMakerNoteExtension->LensShading.shapes.cct2);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvS32(db, "LensShading.shapes.cct3",
         nvCameraMakerNoteExtension->LensShading.shapes.cct3);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvS32(db, "LensShading.shapes.cct1",
         nvCameraMakerNoteExtension->LensShading.shapes.cct1);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32(db, "LensShading.shapes.percent1",
         nvCameraMakerNoteExtension->LensShading.shapes.percent1);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32(db, "LensShading.shapes.percent3",
         nvCameraMakerNoteExtension->LensShading.shapes.percent3);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32(db, "LensShading.shapes.percent2",
         nvCameraMakerNoteExtension->LensShading.shapes.percent2);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertchar(db, "LensShading.shapes.type1",
         nvCameraMakerNoteExtension->LensShading.shapes.type1);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertchar(db, "LensShading.shapes.type3",
         nvCameraMakerNoteExtension->LensShading.shapes.type3);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertchar(db, "LensShading.shapes.type2",
         nvCameraMakerNoteExtension->LensShading.shapes.type2);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32(db, "LensShading.debug.y1",
         nvCameraMakerNoteExtension->LensShading.debug.y1);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32(db, "LensShading.debug.y0",
         nvCameraMakerNoteExtension->LensShading.debug.y0);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32(db, "LensShading.debug.f",
         nvCameraMakerNoteExtension->LensShading.debug.f);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvS32Array(db, "LensShading.PatchDimensions",
        &nvCameraMakerNoteExtension->LensShading.PatchDimensions[0],
        sizeof(nvCameraMakerNoteExtension->LensShading.PatchDimensions));
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertcharArray(db, "LensShading.version",
        &nvCameraMakerNoteExtension->LensShading.version[0],
        sizeof(nvCameraMakerNoteExtension->LensShading.version));
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32Array(db, "OpticalBlack.OpticalBlackValues",
        &nvCameraMakerNoteExtension->OpticalBlack.OpticalBlackValues[0],
        sizeof(nvCameraMakerNoteExtension->OpticalBlack.OpticalBlackValues));
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvBool(db, "AutoFocus.v2.HalfPress",
         nvCameraMakerNoteExtension->AutoFocus.v2.HalfPress);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertcharArray(db, "AutoFocus.v2.AlgorithmStatus",
        &nvCameraMakerNoteExtension->AutoFocus.v2.AlgorithmStatus[0],
        sizeof(nvCameraMakerNoteExtension->AutoFocus.v2.AlgorithmStatus));
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvS32Array(db, "AutoFocus.v2.FocusWindow",
        &nvCameraMakerNoteExtension->AutoFocus.v2.FocusWindow[0],
        sizeof(nvCameraMakerNoteExtension->AutoFocus.v2.FocusWindow));
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvS32(db, "AutoFocus.v2.PositionActualVsPredictedDifference",
         nvCameraMakerNoteExtension->AutoFocus.v2.PositionActualVsPredictedDifference);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvU32Array(db, "AutoFocus.v2.InitialProbingRange",
        &nvCameraMakerNoteExtension->AutoFocus.v2.InitialProbingRange[0],
        sizeof(nvCameraMakerNoteExtension->AutoFocus.v2.InitialProbingRange));
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvU32(db, "AutoFocus.v2.FrameRate",
         nvCameraMakerNoteExtension->AutoFocus.v2.FrameRate);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvS32(db, "AutoFocus.v2.Focus",
         nvCameraMakerNoteExtension->AutoFocus.v2.Focus);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvS32Array(db, "AutoFocus.v2.Timer",
        &nvCameraMakerNoteExtension->AutoFocus.v2.Timer[0],
        sizeof(nvCameraMakerNoteExtension->AutoFocus.v2.Timer));
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32(db, "AutoFocus.v2.SharpnessComputationTime",
         nvCameraMakerNoteExtension->AutoFocus.v2.SharpnessComputationTime);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvS32(db, "AutoFocus.v2.ProbesConsumed",
         nvCameraMakerNoteExtension->AutoFocus.v2.ProbesConsumed);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvS32(db, "AutoFocus.v2.peak",
         nvCameraMakerNoteExtension->AutoFocus.v2.peak);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvF32(db, "AutoFocus.v2.ComputedSharpness",
         nvCameraMakerNoteExtension->AutoFocus.v2.ComputedSharpness);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvS32(db, "AutoFocus.v2.Configuration.RestingPosition",
         nvCameraMakerNoteExtension->AutoFocus.v2.Configuration.RestingPosition);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvS32(db, "AutoFocus.v2.Configuration.FramesSkipped",
         nvCameraMakerNoteExtension->AutoFocus.v2.Configuration.FramesSkipped);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvS32(db, "AutoFocus.v2.Configuration.ProbesPerPass",
         nvCameraMakerNoteExtension->AutoFocus.v2.Configuration.ProbesPerPass);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvS32Array(db, "AutoFocus.v2.Configuration.isp",
        &nvCameraMakerNoteExtension->AutoFocus.v2.Configuration.isp[0],
        sizeof(nvCameraMakerNoteExtension->AutoFocus.v2.Configuration.isp));
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvS32(db, "AutoFocus.v2.Configuration.ActualRange",
         nvCameraMakerNoteExtension->AutoFocus.v2.Configuration.ActualRange);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvS32(db, "AutoFocus.v2.Configuration.Hyperfocal",
         nvCameraMakerNoteExtension->AutoFocus.v2.Configuration.Hyperfocal);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvS32Array(db, "AutoFocus.v2.Configuration.WorkingRange",
        &nvCameraMakerNoteExtension->AutoFocus.v2.Configuration.WorkingRange[0],
        sizeof(nvCameraMakerNoteExtension->AutoFocus.v2.Configuration.WorkingRange));
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvS32(db, "AutoFocus.v2.Configuration.SettleTime",
         nvCameraMakerNoteExtension->AutoFocus.v2.Configuration.SettleTime);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvS32(db, "AutoFocus.v2.Configuration.NumberOfPasses",
         nvCameraMakerNoteExtension->AutoFocus.v2.Configuration.NumberOfPasses);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvS32(db, "AutoFocus.v2.Configuration.v",
         nvCameraMakerNoteExtension->AutoFocus.v2.Configuration.v);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvS32(db, "AutoFocus.v2.Configuration.DynamicProbesThreshold",
         nvCameraMakerNoteExtension->AutoFocus.v2.Configuration.DynamicProbesThreshold);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvS32(db, "AutoFocus.v2.Configuration.DynamicProbes",
         nvCameraMakerNoteExtension->AutoFocus.v2.Configuration.DynamicProbes);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvU32(db, "AutoFocus.v2.ProcessingResult",
         nvCameraMakerNoteExtension->AutoFocus.v2.ProcessingResult);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvU32Array(db, "AutoFocus.v2.SweepRange",
        &nvCameraMakerNoteExtension->AutoFocus.v2.SweepRange[0],
        sizeof(nvCameraMakerNoteExtension->AutoFocus.v2.SweepRange));
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvS32(db, "Configuration.CalibratedCRC",
         nvCameraMakerNoteExtension->Configuration.CalibratedCRC);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvS32(db, "Configuration.OverridesCRC",
         nvCameraMakerNoteExtension->Configuration.OverridesCRC);
    if (e != NvSuccess) {
        return e;
    }

    e = NvCameraJDSInsertNvS32(db, "Configuration.FactoryCRC",
         nvCameraMakerNoteExtension->Configuration.FactoryCRC);
    if (e != NvSuccess) {
        return e;
    }

    return NvSuccess;
}
