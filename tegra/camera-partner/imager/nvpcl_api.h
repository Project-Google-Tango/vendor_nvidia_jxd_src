/**
 * Copyright (c) 2012-2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef PCL_API_H
#define PCL_API_H

#if defined(__cplusplus)
extern "C"
{
#endif

#include "nvpcl_controller.h"
#include "nvcam_frame_data.h"
#include "nvcam_frame_dataitem_ids_private.h"
#include "nvpcl_module_settings.h"
#include "nvpcl_prints.h"

/*
 * The NvPclContext Data structure has information needed by the PCL API
 * functions. After the open function, the handle to the NvPclContext is
 * passed as a parameter for other API functions.
 * @member NvPclQueue: Handle to the PCL API queue of settings.
 * @member hPclController: Handle to the PCL Controller.
 */
typedef struct NvPclContext
{
    NvCamFrameData_Handle hFrameData;
    NvPclControllerHandle hPclController;
    NvPclModuleSetting PrevInputSettings;
} NvPclContext;

typedef struct NvPclContext *NvPclContext_Handle;

/**
 * This creates a new instance of Physical Camera Layer (PCL)
 * A new PCL API context is returned to the calling function for future
 * use of PCL calls to this client. Until a complete transition from NvOdmImager
 * to PCL calls occur, an ImagerGUID and hImager parameter must be used to
 * facilitate a mixture of calls from camera/core being PCL and NvOdmImager
 * @param ImagerGUID: The camera ID to use for front or back
 *      (See Android's CAMERA_FACING_BACK and CAMERA_FACING_FRONT)
 * @param phPcl: The pointer to the handle where the instance will be created
 * @param hImager: A handle to an NvOdmImager instance for PCL to work in
 *      conjunction with; if hImager is NULL, a separate instance will be used
 */
NvError
NvPclOpen(
    NvU64 ImagerGUID,
    NvPclContext_Handle *phPcl,
    NvOdmImagerHandle hImager);

/**
 * NvPclClose to destruct the PCL instance.
 * @param hPcl: The PCL API handle
 */
void NvPclClose(NvPclContext_Handle hPcl);

/**
 * NvPclGetPlatformData is provided to get a snapshot of the platform configuration
 * @param phPcl: The pointer to the handle where the instance will be created
 * @param phData: A handle to a NULL pointer that a platform reference will be assigned
 */
NvError
NvPclGetPlatformData(
    NvPclContext_Handle hPcl,
    NvPclPlatformData **phData);

/**
 * NvPclEstimateSettings is provided as a mechanism to test PCL output effects.
 * It will examine the FrameData for new settings, and provide the output settings
 * PCL would actually resolve.  No settings are applied to hardware in this function.
 * To apply settings to hardware, see NvPclUpdate and NvPclApply functions.
 * @param hPcl: The PCL API handle
 * @param hFrameData is the framedata is examined to extract test settings to be used
 */
NvError
NvPclEstimateSettings(
    NvPclContext_Handle hPcl,
    NvCamFrameData_Handle hFrameData);

/**
 * NvPclUpdate is used to load the latest framedata settings. A handle to the
 * FrameData is passed on. Apply function has to be called to apply the
 * loaded settings.
 * @param hPcl: The PCL API handle
 * @param hFrameData: Handle to the FrameData containing PCL data item settings to be loaded.
 */
NvError
NvPclUpdate(
    NvPclContext_Handle hPcl,
    NvCamFrameData_Handle hFrameData);

/**
 * NvPclApply triggers the first enqueued settings to pass through the PCL layer.
 * Currently does both the "Update" and "Apply" concept. This may change
 * when further frame synching details are realized in camera/core.
 * @param hPcl: The PCL API handle
 */
NvError NvPclApply(NvPclContext_Handle hPcl);

/**
 * NvPclPrintModuleSetting is a debugging print tool to print
 * the given NvPclModuleSetting structure.
 * @param settings: The settings to print
 */
void NvPclPrintModuleSetting(NvPclModuleSetting settings);

/**
 * NvPclPrintRunningModuleInfo is a debugging print tool to print
 * the given NvPclRunningModuleInfo structure.
 * @param info: The module mode info to print
 */
void NvPclPrintRunningModuleInfo(NvPclRunningModuleInfo info);

/**
 * NvPclPrintCameraModuleDefinition is a debugging print tool to print
 * the given NvPclCameraModule structure.
 * @param pModule: The module to print
 */
void NvPclPrintCameraModuleDefinition(NvPclCameraModule *pModule);

#if defined(__cplusplus)
}
#endif

#endif  //PCL_API_H

