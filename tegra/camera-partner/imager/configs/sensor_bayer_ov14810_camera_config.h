/*
* Copyright (c) 2011 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software and related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA Corporation is strictly prohibited.
*/
#ifndef SENSOR_BAYER_CAMERA_CONFIG_H
#define SENSOR_BAYER_CAMERA_CONFIG_H
#if defined(__cplusplus)
extern "C"
{
#endif
// GENERATED FILE - DO NOT MODIFY!
static const char *pSensorCalibrationData = {
    "##\n"
    "## Autocalibration\n"
    "## Sensor  :   calibration_runs\n"
    "## Size    :   (4416, 3312)\n"
    "## Version :   OV14810\n"
    "## Generated : Sep 30 2011\n"
    "## \n"
    "\n"
    "\n"
    "## global configurations\n"
    "##\n"
    "global.configType = 1;\n"
    "global.configVersion = 0x100;\n"
    "\n"
    "## isp functionality attributes\n"
    "## - Some blocks have detailed control entries below.\n"
    "## - Turning a block's attribute to FALSE will cause corresponding control entries to be ignored. \n"
    "##\n"
    "ap15Function.opticalBlack = FALSE;\n"
    "ap15Function.deknee = FALSE;\n"
    "ap15Function.lensShading = FALSE;\n"
    "ap15Function.badPixel = FALSE;\n"
    "ap15Function.noiseControl1 = FALSE;\n"
    "ap15Function.demosaic = TRUE;\n"
    "ap15Function.programmableDemosaic = FALSE;\n"
    "ap15Function.colorArtifactReduction = FALSE;\n"
    "ap15Function.noiseControl2 = FALSE;\n"
    "ap15Function.colorCorrection = FALSE;\n"
    "ap15Function.edgeEnhance = FALSE;\n"
    "ap15Function.gammaCorrection = TRUE;\n"
    "\n"
    "\n"
    "anr.LumaThreshold0   = 0;\n"
    "anr.ChromaThreshold0 = 0.08;\n"
    "anr.LumaSlope = 0;\n"
    "anr.ChromaSlope = 0.4;\n"
    "\n"
    "\n"
};
#if defined(__cplusplus)
}
#endif
#endif //SENSOR_BAYER_CAMERA_CONFIG_H
