/**
 * Copyright (c) 2012-2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef PCL_NVODM_TOOLS_H
#define PCL_NVODM_TOOLS_H

#if defined(__cplusplus)
extern "C"
{
#endif

#include "nvpcl_driver.h"
#include "nvpcl_controller.h"
#include "nvpcl_module_settings.h"

#define PCL_NVODM_OBJ_VERSION (0)

NvError NvPclNvOdmApplyState(NvPclControllerHandle hPclController);


NvError NvPclNvOdmQueryStateSensor(NvPclDriver *pDriver, NvPclModuleSetting *pOutputSettings);

NvError NvPclNvOdmQueryStateFocuser(NvPclDriver *pDriver, NvPclModuleSetting *pOutputSettings);

NvError NvPclNvOdmQueryStateFlash(NvPclDriver *pDriver, NvPclModuleSetting *pOutputSettings);

NvError NvPclDriverInitializeDataNvOdmLensStub(NvPclCameraModuleHandle hModule, NvPclDriver *pLensDriver);

NvError NvPclDriverInitializeDataNvOdmSensor(NvPclCameraModuleHandle hModule, NvPclDriver *pSensorDriver);
NvError NvPclDriverInitializeDataNvOdmFocuser(NvPclCameraModuleHandle hModule, NvPclDriver *pFocuserDriver);
NvError NvPclDriverInitializeDataNvOdmFlash(NvPclCameraModuleHandle hModule, NvPclDriver *pFlashDriver);

void NvPclNvOdmCloseDriver(struct NvPclDriverRec *pPclDriver);

#if defined(__cplusplus)
}
#endif

#endif  //PCL_NVODM_TOOLS_H

