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
#include "nvpcl_controller_funcs.h"
#include "nvpcl_nvodm_tools.h"

#include "nvpcl_driver_sensor.h"
#include "nvpcl_driver_focuser.h"
#include "nvpcl_driver_flash.h"

#include "nvpcl_nvodm_hals.h"
#include "nvpcl_driver_hals.h"
#include "imager_util.h"

#if NV_ENABLE_DEBUG_PRINTS
#define PCL_DB_PRINTF(...)  NvOsDebugPrintf(__VA_ARGS__)
#else
#define PCL_DB_PRINTF(...)
#endif

NvError NvPclDriverGetVolatileSettings(NvPclDriver *pDriver, NvPclModuleSetting *pSettings) {
    NvError status = NvSuccess;

    if((!pDriver) || (!pSettings)) {
        NvOsDebugPrintf("%s: Passed in NULL parameters\n", __func__);
        return NvError_BadParameter;
    }

    if(pDriver->pfnPclDriverGetVolatile) {
        status = pDriver->pfnPclDriverGetVolatile(pDriver, pSettings);
        if(status != NvSuccess) {
            NvOsDebugPrintf("%s: %s: unable to read available driver volatile settings\n",
                __func__, pDriver->Profile.HwDev.Name);
        }
    }

    return status;
}

NvError NvPclReloadVolatileInfo(NvPclControllerHandle hPclController) {
    NvPclModuleSetting *pWorkingSettings = NULL;
    NvPclCameraModule *pModule = NULL;
    NvU8 i = 0;

    if(!hPclController) {
        NvOsDebugPrintf("%s: Attempted to get state without initialization\n", __func__);
        return NvError_BadParameter;
    }

    pWorkingSettings = &(hPclController->pPrivateContext->AppliedState.OutputSettings);
    pModule = hPclController->PlatformData.hFirstActiveModule;

    for(i = 0; i < pModule->NumSubDrivers; i++) {
        NvPclDriverGetVolatileSettings(&pModule->SubDriver[i], pWorkingSettings);
    }

    return NvSuccess;
}

NvError NvPclGetDriverUpdateFunctions(NvPclPackageList *pPackageList, NvPclCameraModule *pModule) {
    NvError status;
    NvPclControlPackage *pPackages = NULL;
    NvU32 numPackages = 0;
    NvU32 totalPkgCount = 0;
    NvU32 pkgIndex  = 0;
    NvU32 regCount = 0;
    NvU32 i = 0;
    NvU32 j = 0;

    if((!pPackageList) || (!pModule)) {
        NvOsDebugPrintf("%s: NvPcl (Module||Package list) was passed empty\n", __func__);
        return NvError_BadParameter;
    }

    for(i = 0; i < pModule->NumSubDrivers; i++) {
        if(!pModule->SubDriver[i].pfnPclDriverGetUpdateFun) {
            NvOsDebugPrintf("%s: NvPcl Drivers have not been initialized properly\n", __func__);
            return NvError_InvalidState;
        }

        numPackages = 0;
        status = pModule->SubDriver[i].pfnPclDriverGetUpdateFun(&pModule->SubDriver[i], NULL, &numPackages);
        if(status != NvSuccess) {
            NvOsDebugPrintf("%s: Failed to grab driver '%s' update functions\n",
                __func__, pModule->SubDriver[i].Profile.HwDev.Name);
           return status;
        }

        PCL_DB_PRINTF("%s: Driver '%s' requesting %d packages\n",
            __func__, pModule->SubDriver[i].Profile.HwDev.Name, numPackages);

        totalPkgCount += numPackages;
    }

    PCL_DB_PRINTF("%s: Module package allocating '%d'\n", __func__, totalPkgCount);

    // Allocating the package memory

    pPackages = (NvPclControlPackage *)NvOsAlloc(totalPkgCount*sizeof(NvPclControlPackage));
    if(!pPackages) {
        NvOsDebugPrintf("%s: NvOsAlloc failure on Package List\n", __func__);
        return NvError_InsufficientMemory;
    }

    NvOsMemset(pPackages, 0, totalPkgCount*sizeof(NvPclControlPackage));
    pkgIndex = 0;

    if(totalPkgCount != 0) {
        for(i = 0; i < pModule->NumSubDrivers; i++) {
            numPackages = 0;
            pPackages[pkgIndex].HwBlob.dev = pModule->SubDriver[i].Profile.HwDev;
            status = pModule->SubDriver[i].pfnPclDriverGetUpdateFun(&pModule->SubDriver[i],
                        &(pPackages[pkgIndex]), &numPackages);
            if(status != NvSuccess) {
                NvOsDebugPrintf("%s: Failed to grab driver '%s' update functions\n",
                    __func__, pModule->SubDriver[i].Profile.HwDev.Name);
                goto getupdatefun_fail;
            }
            for(j = 0; j < numPackages; j++) {
                regCount += pPackages[pkgIndex].HwBlob.regList.regdata_length;
                pkgIndex++;
            }
        }
    }

    if(pkgIndex != totalPkgCount) {
        NvOsDebugPrintf("%s: Package sanity count failed (%u != %u)\n", __func__, pkgIndex, totalPkgCount);
    }

    pPackageList->numPackages = totalPkgCount;
    pPackageList->pSequence = pPackages;
    pPackageList->numRegisters = regCount;

    return NvSuccess;

getupdatefun_fail:

    NvOsFree(pPackages);
    return status;

}

// This function only saves cleanliness at this point...
NvError NvPclDriverInitializeData(NvPclCameraModuleHandle hModule, NvPclDriver *pDriver) {
    NvError status;

    if((!hModule) || (!pDriver) || (!pDriver->pfnPclDriverInitialize)) {
        NvOsDebugPrintf("%s: initialize with invalid parameters\n", __func__);
        return NvError_BadParameter;
    }

    status = pDriver->pfnPclDriverInitialize(hModule, pDriver);
    if(status != NvSuccess) {
        NvOsDebugPrintf("%s: Unable to initialize driver %s\n",
            __func__, pDriver->Profile.HwDev.Name);
        // Not returning error here to help legacy auto detection issues
    }

    return NvSuccess;
}

NvError NvPclInitializeDrivers(NvPclCameraModule *pModule) {
    NvError status;
    NvBool result;
    NvU32 i = 0;
    NvBool hasFocuser = NV_FALSE;

    if(!pModule) {
        NvOsDebugPrintf("%s: pModule == NULL\n", __func__);
        return NvError_BadParameter;
    }

    // Set Imager PowerLevel On for safety
    result = NvOdmImagerSetPowerLevel(pModule->hImager, NvOdmImagerDeviceMask_All, NvOdmImagerPowerLevel_On);
    if(!result) {
        NvOsDebugPrintf("%s: warning: failed to set NvOdmImager power level ON\n", __func__);
    }

    for(i = 0; i < pModule->NumSubDrivers; i++) {
        PCL_DB_PRINTF("%s: %s ++++++++++++++++++\n", __func__, pModule->SubDriver[i].Profile.HwDev.Name);
        status = NvPclDriverInitializeData(pModule, &pModule->SubDriver[i]);
        if(status != NvSuccess) {
            NvOsDebugPrintf("%s: error: Failed to init camera sub module %s\n",
                __func__);
        }
        if(pModule->SubDriver[i].Profile.Type == NvPclDriverType_Focuser)
            hasFocuser = NV_TRUE;
        PCL_DB_PRINTF("%s: %s ------------------\n", __func__, pModule->SubDriver[i].Profile.HwDev.Name);
    }

    /**
     * Temporary stub driver hack until lens properties are in config data
     */
    if(hasFocuser == NV_FALSE) {
        pModule->SubDriver[pModule->NumSubDrivers].pfnPclDriverInitialize =
                                                    NvPclDriverInitializeDataNvOdmLensStub;
        status = NvPclDriverInitializeData(pModule, &pModule->SubDriver[pModule->NumSubDrivers]);
        if(status != NvSuccess) {
            NvOsDebugPrintf("%s: error: failed to load backup lens properties\n", __func__);
        }
        pModule->NumSubDrivers++;
    }

    return NvSuccess;
}

NvError NvPclDriverGetUpdateDefault(NvPclDriver *pPclDriver, NvPclControlPackageHandle packages, NvU32 *numPackages)
{
    if(!pPclDriver) {
        NvOsDebugPrintf("%s: Passed in Null NvPcl Driver\n", __func__);
        return NvError_BadParameter;
    }
    *numPackages = 0;

    return NvSuccess;
}

NvError
NvPclGetDriverTypeIndex(
    NvPclCameraModule *pModule,
    NvPclDriverType type,
    NvU32 *pIndex)
{
    NvU32 i = 0;

    for(i = 0; i < pModule->NumSubDrivers; i++) {
        if(pModule->SubDriver[i].Profile.Type == type) {
            *pIndex = i;
            return NvSuccess;
        }
    }

    return NvError_ModuleNotPresent;
}

NvError
NvPclGuidInTable(
    NvOdmGUIDNameEntry *tableGUID,
    NvU32 tableSize,
    NvU64 GUID)
{
    NvU32 i = 0;

    for(i = 0; i < tableSize; i++) {
        if(tableGUID[i].GUID == GUID) {
            return NvSuccess;
        }
    }

    return NvError_BadParameter;
}

NvError
NvPclNameToGuid(
    NvOdmGUIDNameEntry *tableGUID,
    NvU32 tableSize,
    const char *name,
    NvU64 *pGUID)
{
    NvU32 i = 0;
    *pGUID = 0;

    for(i = 0; i < tableSize; i++) {
        if(NvOsStrcmp(tableGUID[i].dev_name, name) == 0) {
            *pGUID = tableGUID[i].GUID;
            return NvSuccess;
        }
    }

    return NvError_BadParameter;
}

/**
 * Starts a new driver instance, assigns HwBlob to it, assign driver to module camera type (sensor/focuser/flash)
 * If hwblob subname (from kernel) is nvodm, GUID is looked up and assigned to pGUID
 * new NvPcl drivers have GUID of 0
 */
NvError NvPclCreateDriver(NvPclDriver *pDriver, NvPclHwCameraSubModule subHwDev, NvU64 *pGUID) {
    NvBool halFound = NV_FALSE;
    NvU64 GUID = 0;

    // Clean instance of NvPclDriver
    NvOsMemset(pDriver, 0, sizeof(NvPclDriver));

    pDriver->Profile.HwDev = subHwDev;
    pDriver->pfnPclDriverGetUpdateFun = NvPclDriverGetUpdateDefault;

    if(NvOsStrcmp(subHwDev.Name, PCL_STR_LIST_END) == 0) {
        NvOsDebugPrintf("%s: error: an empty driver instance was found\n", __func__);
        return NvError_BadParameter;
    }

    /**
     * First try to match driver name to a NvPcl driver in new HAL table
     */
// Disabling for now until we can reliably prove NvPcl drivers
#if 0
    for(i = 0; i < NV_ARRAY_SIZE(g_PclDevNameHALTable); i++) {
        if(NvOsStrcmp(subHwDev.Name, g_PclDevNameHALTable[i].dev_name) == 0) {
            NvOsDebugPrintf("%s: Found NvPcl Driver Hal dev_name match (%s)\n", __func__, subHwDev.Name);
            halFound = NV_TRUE;
            pDriver->Profile.IsOldNvOdmDriver = NV_FALSE;
            GUID = 0;
            status = g_PclDevNameHALTable[i].pfnGetHal(pDriver);
            if(status != NvSuccess) {
                NvOsDebugPrintf("%s: Failure to retrieve NvPcl Driver HAL\n", __func__);
                return status;
            }
            break;
        }
    }
#endif
    /**
     * Next try to match driver name to legacy sensor HAL table
     */
    if(!halFound) {
        PCL_DB_PRINTF("%s: Unable to find a NvPcl Driver name match (%s)\n", __func__, subHwDev.Name);
        PCL_DB_PRINTF("%s: Checking legacy driver list...\n", __func__);

        NvPclNameToGuid(g_PclNvOdmSensorGUIDTable,
                        NV_ARRAY_SIZE(g_PclNvOdmSensorGUIDTable),
                        subHwDev.Name,
                        &GUID);
        if(GUID != 0) {
            halFound = NV_TRUE;
            pDriver->Profile.IsOldNvOdmDriver = NV_TRUE;
            pDriver->pfnPclDriverInitialize = NvPclDriverInitializeDataNvOdmSensor;
        }
    }

    /**
     * Next try to match driver name to legacy focuser HAL table
     */
    if(!halFound) {
        NvPclNameToGuid(g_PclNvOdmFocuserGUIDTable,
                        NV_ARRAY_SIZE(g_PclNvOdmFocuserGUIDTable),
                        subHwDev.Name,
                        &GUID);
        if(GUID != 0) {
            halFound = NV_TRUE;
            pDriver->Profile.IsOldNvOdmDriver = NV_TRUE;
            pDriver->pfnPclDriverInitialize = NvPclDriverInitializeDataNvOdmFocuser;
        }
    }

    /**
     * Next try to match driver name to legacy flash HAL table
     */
    if(!halFound) {
        NvPclNameToGuid(g_PclNvOdmFlashGUIDTable,
                        NV_ARRAY_SIZE(g_PclNvOdmFlashGUIDTable),
                        subHwDev.Name,
                        &GUID);
        if(GUID != 0) {
            halFound = NV_TRUE;
            pDriver->Profile.IsOldNvOdmDriver = NV_TRUE;
            pDriver->pfnPclDriverInitialize = NvPclDriverInitializeDataNvOdmFlash;
        }
    }

    if(!halFound) {
        NvOsDebugPrintf("%s: Unable to find a Driver name match (%s)\n", __func__, subHwDev.Name);
        return NvError_BadParameter;
    } else {
        PCL_DB_PRINTF("%s: Found a Driver name match (%s)\n", __func__, subHwDev.Name);
    }

    *pGUID = GUID;

    return NvSuccess;
}

/**
 * pModule driver instances will be allocated to pModule on success
 * pModule driver instances will have the HwBlob initialized
 */
NvError NvPclConnectDrivers(NvPclCameraModule *pModule) {
    NvError status, e;
    NvBool result;
    NvPclHwCameraModule *pHwModule = NULL;
    NvU8 i;

    if(!pModule) {
        NvOsDebugPrintf("%s: passed in a NULL module\n", __func__);
        return NvError_BadParameter;
    }

    pHwModule = &pModule->HwModuleDef;

    /**
     * GUIDs are not technically necessary for NvPcl drivers to work,
     * But should be 0 to avoid Imager Driver's HAL registerations
     * See NvPclConnectDriverProfile() for implementation details
     */
    NvU64 SensorGUID = 0;
    NvU64 FocuserGUID = 0;
    NvU64 FlashGUID = 0;
    NvU64 GUID = 0;

    for(i = 0; i < MAX_NUM_CAM_SUBMODULES ; i++) {
        if(NvOsStrcmp(pHwModule->HwCamSubModule[i].Name, PCL_STR_LIST_END) == 0) {
            break;
        }
        GUID = 0;

        NV_CAM_CHECK_SHOW_ERROR_CLEANUP(
            NvPclCreateDriver(&pModule->SubDriver[i], pHwModule->HwCamSubModule[i], &GUID));
        pModule->SubDriver[i].hParentModule = pModule;

        // These GUIDs only applicable to legacy NvOdm GUID structure
        if(GUID != 0) {
            if(NvSuccess == NvPclGuidInTable(
                                g_PclNvOdmSensorGUIDTable,
                                NV_ARRAY_SIZE(g_PclNvOdmSensorGUIDTable),
                                GUID)) {
                SensorGUID = GUID;
            } else if(NvSuccess == NvPclGuidInTable(
                                g_PclNvOdmFocuserGUIDTable,
                                NV_ARRAY_SIZE(g_PclNvOdmFocuserGUIDTable),
                                GUID)) {
                FocuserGUID = GUID;
            } else if(NvSuccess == NvPclGuidInTable(
                                g_PclNvOdmFlashGUIDTable,
                                NV_ARRAY_SIZE(g_PclNvOdmFlashGUIDTable),
                                GUID)) {
                FlashGUID = GUID;
            }

        }
    }
    pModule->NumSubDrivers = i;

    /**
     * Initialize Imager for legacy drivers
     */

    /**
     * GUIDs at this point should still be 0 if they are using NvPcl drivers
     * (SensorGUID of 0 forces a "virtual null" sensor in NvOdmImager)
     */

    /**
     * To help facilitate a smoother transition to NvPcl from NvOdmImager,
     * we add a check to see if we were provided an already instantiated
     * NvOdmImager handle; otherwise, create one.
     */
    if((pModule->Activated) && (pModule->hImager == NULL)) {
        PCL_DB_PRINTF("%s: hImager was NULL, creating new imager\n", __func__);
        NvBool UseSensorCapGUIDs = NV_FALSE;
        pModule->hImagerNeedFreeing = NV_TRUE;
        result = NvOdmImagerOpenExpanded(SensorGUID, FocuserGUID, FlashGUID, UseSensorCapGUIDs, &(pModule->hImager));
        if(!result) {
            NvOsDebugPrintf("%s: Failed to open NvOdmImager (%X, %X, %X) - UseSensorCapGUIDS : %u\n",
                __func__, SensorGUID, FocuserGUID, FlashGUID, UseSensorCapGUIDs);
            status = NvError_BadParameter;
            goto fail;
        }
    } else {
        pModule->hImagerNeedFreeing = NV_FALSE;
    }

    return NvSuccess;

fail:
    NvOsDebugPrintf("%s: was unable to create instance of drivers\n", __func__);

    if(pModule->hImager) {
        if(pModule->hImagerNeedFreeing) {
            NvOdmImagerClose(pModule->hImager);
        }
    }

    return status;
}

NvError NvPclLoadCalibrations(NvPclCameraModule *pModule, NvPclHwCameraModule *pHwModule) {

    if((!pModule) || (!pHwModule)) {
        NvOsDebugPrintf("%s: Passed null pointers\n", __func__);
        return NvError_BadParameter;
    }

    if(pHwModule->CalibrationData) {
        pModule->CalibrationData.Size = NvOsStrlen(pHwModule->CalibrationData)+1;
        pModule->CalibrationData.pData = NvOsRealloc(pModule->CalibrationData.pData,
                                                    pModule->CalibrationData.Size);
        NvOsMemcpy(pModule->CalibrationData.pData, pHwModule->CalibrationData,
                                                pModule->CalibrationData.Size);
    }

    if(pHwModule->CalibrationOverride[0]) {
        pModule->CalibrationOverride.pData = LoadOverridesFile(pHwModule->CalibrationOverride,
                                                        NV_ARRAY_SIZE(pHwModule->CalibrationOverride));
        if(pModule->CalibrationOverride.pData != NULL) {
            pModule->CalibrationOverride.Size = NvOsStrlen(pModule->CalibrationOverride.pData)+1;
        }
    }

    if(pHwModule->FactoryCalibration[0]) {
        pModule->FactoryCalibration.Size = MAX_FLASH_SIZE;
        pModule->FactoryCalibration.pData = NvOsRealloc(pModule->FactoryCalibration.pData,
                                                        pModule->FactoryCalibration.Size);
        LoadBlobFile(pHwModule->FactoryCalibration,
                        NV_ARRAY_SIZE(pHwModule->FactoryCalibration),
                        pModule->FactoryCalibration.pData,
                        pModule->FactoryCalibration.Size);
    }

    return NvSuccess;
}

NvError NvPclStartPlatformDrivers(NvPclPlatformData *pData) {
    NvError e;
    NvU8 i = 0;

    if(!pData) {
        NvOsDebugPrintf("%s: Passed null platform data\n", __func__);
        return NvError_BadParameter;
    }

    for(i = 0; i < pData->NumModules; i++) {
        NV_CAM_CHECK_SHOW_ERROR_CLEANUP(
            NvPclConnectDrivers(&pData->ModuleList[i]));

        if(pData->ModuleList[i].Activated == NV_TRUE) {
            NV_CAM_CHECK_SHOW_ERROR_CLEANUP(
                NvPclInitializeDrivers(&(pData->ModuleList[i])));
        }
    }

    return NvSuccess;

fail:
    NvOsDebugPrintf("%s: Failed to start module drivers\n", __func__);
    for(i = 0; i < pData->NumModules; i++) {
        NvPclCloseModuleDrivers(&(pData->ModuleList[i]));
    }

    return e;
}

void NvPclCloseModuleDrivers(NvPclCameraModule *pModule) {
    NvU8 i = 0;

    if(!pModule)
        return;

    for(i = 0; i < pModule->NumSubDrivers; i++) {
        if(pModule->SubDriver[i].pfnPclDriverClose) {
            pModule->SubDriver[i].pfnPclDriverClose(&pModule->SubDriver[i]);
        }
    }
    if(pModule->CalibrationData.pData) {
        NvOsFree(pModule->CalibrationData.pData);
    }
    if(pModule->CalibrationOverride.pData) {
        NvOsFree(pModule->CalibrationOverride.pData);
    }
    if(pModule->FactoryCalibration.pData) {
        NvOsFree(pModule->FactoryCalibration.pData);
    }

    if(pModule->hImager) {
        if(pModule->hImagerNeedFreeing) {
            NvOdmImagerClose(pModule->hImager);
        }
    }

    return;
}

NvError NvPclSetupRegisterMemory(NvPclController *pPclController) {
    StateControllerContext *pContext;
    NvPclControlPackage *pControlPackage = NULL;
    NvPclHwReg *pRegisterListApplied = NULL;
    NvPclHwReg *pRegisterListConfig = NULL;
    NvU32 totalHwRegSize = 0;
    NvU32 offset = 0;
    NvU32 i = 0;

    if((!pPclController) || (!pPclController->pPrivateContext)) {
        NvOsDebugPrintf("%s: pPclController was not setup properly yet\n", __func__);
        return NvError_BadParameter;
    }
    pContext = pPclController->pPrivateContext;

    totalHwRegSize = (sizeof(NvPclHwReg))*(pContext->HWPackages.numRegisters);

    NV_CAM_ALLOC_AND_ERROR_CLEANUP(pRegisterListConfig, sizeof(totalHwRegSize));
    NvOsMemset(pRegisterListConfig, 0, totalHwRegSize);
    pContext->ConfigurationState.pHWConfigMemory = pRegisterListConfig;

    NV_CAM_ALLOC_AND_ERROR_CLEANUP(pRegisterListApplied, sizeof(totalHwRegSize));
    NvOsMemset(pRegisterListApplied, 0, totalHwRegSize);
    pContext->AppliedState.pHWConfigMemory = pRegisterListApplied;

    /**
     * Assign the register pointers in the package
     * list to the newly allocated memory
     */

    offset = 0;
    for(i = 0; i < pContext->HWPackages.numPackages; i++) {
        pControlPackage = &(pContext->HWPackages.pSequence[i]);
        pControlPackage->HwBlob.regList.regdata_p = pContext->ConfigurationState.pHWConfigMemory+offset;
        offset += (pControlPackage->HwBlob.regList.regdata_length)*sizeof(NvPclHwReg);
    }

    return NvSuccess;

fail:
    NvOsDebugPrintf("%s: Failed to setup register memory\n", __func__);

    return NvError_InsufficientMemory;
}

