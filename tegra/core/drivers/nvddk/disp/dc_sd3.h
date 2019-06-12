/*
 * Copyright (c) 2007 - 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_DC_SD3_H
#define INCLUDED_DC_SD3_H

#include "t30/ardisplay.h"
#include "nvddk_disp_hw.h"

#define SD_LUT_SIZE     9
#define SD_HISTO_SIZE   8
#define SD_BL_TF_SIZE   4

typedef struct SmartDimmerLogEntryRec
{
    // Registers
    NvU32        SD_CONTROL;
    NvU32        SD_CSC_COEFF;
    NvU32        SD_LUT[SD_LUT_SIZE];
    NvU32        SD_FLICKER_CONTROL;
    NvU32        SD_PIXEL_COUNT;
    NvU32        SD_HISTOGRAM[SD_HISTO_SIZE];
    NvU32        SD_BL_PARAMETERS;
    NvU32        SD_BL_TF[SD_BL_TF_SIZE];
    NvU32        SD_BL_CONTROL;
    NvU32        SD_HW_K_VALUES;

    // Display attributes
    NvU32        InBacklightIntensity;
    NvU32        OutBacklightIntensity;

} SmartDimmerLogEntry;

#if defined(__cplusplus)
extern "C"
{
#endif

void DcHal_HwSmartDimmerConfig( NvU32 controller,
    NvDdkSmartDimmerSettings *sdSettings, NvBool *manual );
void DcHal_HwSmartDimmerBrightness( NvU32 controller, NvU8 *brightness );

void DcHal_HwSmartDimmerLog( NvU32 controller, NvU32 InIntensity,
                             NvU32 OutIntensity, NvBool show );
#if defined(__cplusplus)
}
#endif

#endif
