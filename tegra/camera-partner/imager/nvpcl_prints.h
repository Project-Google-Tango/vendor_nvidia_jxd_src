/**
 * Copyright (c) 2014 NVIDIA Corporation.  All rights reserved.
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

void NvPclPrintModuleSetting(NvPclModuleSetting settings);

void NvPclPrintRunningModuleInfo(NvPclRunningModuleInfo info);

void NvPclPrintSensorObject(NvPclSensorObject *pMode);

void NvPclPrintFocuserObject(NvPclFocuserObject *pMode);

void NvPclPrintFlashObject(NvPclFlashObject *pMode);

void NvPclPrintCameraModuleDefinition(NvPclCameraModule *pModule);

