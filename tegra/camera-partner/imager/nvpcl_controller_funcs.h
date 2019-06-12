/**
 * Copyright (c) 2013-2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef PCL_CONTROLLER_FUNCS_H
#define PCL_CONTROLLER_FUNCS_H

#if defined(__cplusplus)
extern "C"
{
#endif

#include "nvos.h"
/**
 *  NvPcl includes
 */
#include "nvpcl_driver.h"

NvError NvPclLoadCalibrations(NvPclCameraModule *pModule, NvPclHwCameraModule *pHwModule);

NvError NvPclStartPlatformDrivers(NvPclPlatformData *pData);

void NvPclCloseModuleDrivers(NvPclCameraModule *pModule);

NvError NvPclDriverGetVolatileSettings(NvPclDriver *pDriver, NvPclModuleSetting *pSettings);

NvError NvPclReloadVolatileInfo(NvPclControllerHandle hPclController);

NvError NvPclGetDriverUpdateFunctions(NvPclPackageList *pPackageList, NvPclCameraModule *pModule);

NvError NvPclDriverInitializeData(NvPclCameraModuleHandle hModule, NvPclDriver *pDriver);

NvError NvPclInitializeDrivers(NvPclCameraModule *pModule);

NvError NvPclDriverGetUpdateDefault(NvPclDriver *pPclDriver, NvPclControlPackageHandle packages, NvU32 *numPackages);

NvError NvPclGetDriverTypeIndex(NvPclCameraModule *pModule, NvPclDriverType type, NvU32 *pIndex);

NvError NvPclGuidInTable(NvOdmGUIDNameEntry *tableGUID, NvU32 tableSize, NvU64 GUID);

NvError NvPclNameToGuid(NvOdmGUIDNameEntry *tableGUID, NvU32 tableSize, const char *name, NvU64 *pGUID);

NvError NvPclCreateDriver(NvPclDriver *pDriver, NvPclHwCameraSubModule subHwDev, NvU64 *pGUID);

NvError NvPclConnectDrivers(NvPclCameraModule *pModule);

NvError NvPclSetupRegisterMemory(NvPclController *pPclController);

#if defined(__cplusplus)
}
#endif

#endif  //PCL_CONTROLLER_FUNCS_H

