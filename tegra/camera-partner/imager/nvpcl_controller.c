/**
 * Copyright (c) 2013-2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


#include "nvpcl_controller.h"
#include "nvpcl_controller_funcs.h"
#include "nvpcl_nvodm_tools.h"

#include "nvpcl_driver.h"
#include "nvpcl_driver_sensor.h"
#include "nvpcl_driver_focuser.h"
#include "nvpcl_driver_flash.h"
#include "nvpcl_module_settings.h"

#if NV_ENABLE_DEBUG_PRINTS
#define PCL_DB_PRINTF(...)  NvOsDebugPrintf(__VA_ARGS__)
#else
#define PCL_DB_PRINTF(...)
#endif

/**
 * Open is the initializing function that will create a new Controller instance based
 * on the ImagerGUID provided.
 * @param phPclController is the handle where a new instance will be assigned. Must be
 *                          passed in NULL.
 * @param ImagerGUID is the camera module setup to be selected.
 * @param hImager is the optional reference to an existing NvOdmImagerHandle. This is
 *              intended to facilitate an easier NvPcl integration.  If NULL is passed,
 *              NvPcl will create its own instance of NvOdmImager.
 */
NvError
NvPclStateControllerOpen(
    NvU64 ImagerGUID,
    NvPclControllerHandle *phPclController,
    NvOdmImagerHandle hImager)
{
    NvError e = NvSuccess;
    StateControllerContext *pContext = NULL;
    NvPclController *pPclController = NULL;
    NvBool isVirtual = NV_FALSE;

    NvPclHwCameraModule pHwModules[PCL_MAX_NUM_MODULES]; // Will contain list of available modules on hw platform
    NvPclHwCameraModule *pActiveHwModule;
    NvU32 numModules = 0; // Number of names in HwModuleNames array
    NvU32 i = 0;

    if(*phPclController != NULL) {
        NvOsDebugPrintf("%s: error: NvPclHandle reference was not NULL\n", __func__);
        return NvError_BadParameter;
    }

    NV_CAM_ALLOC_AND_ERROR_CLEANUP(pPclController, sizeof(NvPclController));
    NvOsMemset(pPclController, 0, sizeof(NvPclController));

    NV_CAM_ALLOC_AND_ERROR_CLEANUP(pContext, sizeof(StateControllerContext));
    NvOsMemset(pContext, 0, sizeof(StateControllerContext));

    *phPclController = pPclController;
    pPclController->pPrivateContext = pContext;

    /**
     * Start establishing target camera hardware
     */

    e = NvPclHwGetModuleList(NULL, &numModules, isVirtual);
    if(e == NvError_NotSupported) {
        NvOsDebugPrintf("%s: device detection on this platform is not supported\n", __func__);
    }

    if(numModules == 0) {
        NvOsDebugPrintf("%s: device detection worked, but no modules on platform\n\n", __func__);
        return NvSuccess;
    }

    if(PCL_MAX_NUM_MODULES < numModules) {
        NvOsDebugPrintf("%s: error: numModules returned > module list memory.\n", __func__);
        goto fail;
    }

    if(ImagerGUID > numModules) {
        // Assume a virtual sensor for now. We only provide a stub.
        NvOsDebugPrintf("%s: ImagerGUID (%d) exceeds available imagers (%d), assuming virtual\n",
            __func__, ImagerGUID, numModules);
        isVirtual = NV_TRUE;
    }

    NvOsMemset(pHwModules, 0, sizeof(pHwModules));
    e = NvPclHwGetModuleList(pHwModules, &numModules, isVirtual);
    if(e == NvError_NotSupported) {
        NvOsDebugPrintf("%s: attempting to use backup default module list\n", __func__);
    }

    for(i = 0; i < numModules; i++) {
        PCL_DB_PRINTF("%s: NvPclHwModule list[%d]: %s\n", __func__, i, pHwModules[i].Name);
    }

    if(isVirtual == NV_TRUE) {
        return NvSuccess;
    } else {
        pActiveHwModule = &(pHwModules[ImagerGUID]);

        // Setting up kernel driver
        NV_CAM_CHECK_SHOW_ERROR_CLEANUP(NvPclHwInitializeModule(pActiveHwModule));
    }

    /**
     * Initialize NvPcl driver software
     */
    pPclController->PlatformData.NumModules = numModules;
    for(i = 0; i < numModules; i++) {
        NvOsMemcpy(&(pPclController->PlatformData.ModuleList[i].HwModuleDef),
            &(pHwModules[i]), sizeof(NvPclHwCameraModule));
    }

    pPclController->PlatformData.hFirstActiveModule = &pPclController->PlatformData.ModuleList[ImagerGUID];
    pPclController->PlatformData.hFirstActiveModule->Activated = NV_TRUE;
    pPclController->PlatformData.hFirstActiveModule->hImager = hImager;

    NV_CAM_CHECK_SHOW_ERROR_CLEANUP(NvPclStartPlatformDrivers(&(pPclController->PlatformData)));
/*
    NV_CAM_CHECK_SHOW_ERROR_CLEANUP(NvPclGetDriverUpdateFunctions(&(pContext->HWPackages), pModule));
*/

    return NvSuccess;

fail:
    NvOsDebugPrintf("%s: Failed. (error 0x%X)\n", __func__, e);

    NvPclStateControllerClose(pPclController);

    return NvError_ResourceError;
}

/**
 * GetPlatformData is a querying function for initial, static parameters.
 * @param hPclController is the instance to query
 * @param phData is a handle to a NULL pointer to where the information will be assigned
 */
NvError
NvPclStateControllerGetPlatformData(
    const NvPclControllerHandle hPclController,
    NvPclPlatformData **phData)
{
    if(!hPclController) {
        NvOsDebugPrintf("%s: Attempted to list info before modules have been initialized\n", __func__);
        return NvError_InvalidState;
    }
    if(!phData) {
        NvOsDebugPrintf("%s: error: Expected a PlatformData handle pointer\n");
        return NvError_BadParameter;
    }

    *phData = &(hPclController->PlatformData);

    return NvSuccess;
}

/**
 * Update function is used to test a configuration of settings
 * @param hPclController is the instance to test
 * @param input is the configuration to test
 * @param output is the resulting configuration of the test, exposing any limitations of the driver instances
 * @param PackageRegs is a boolean for when true, StateSettings and corresponding register values are
 *                  kept active in the Controller sequence (and will be used in the next Apply()); otherwise
 *                  false will not store any information in the controller context.
 *                  NOTE: Controller only has a single cache of input settings that would be used for Apply()
 */
NvError
NvPclStateControllerUpdate(
    NvPclControllerHandle hPclController,
    const NvPclModuleSetting *input,
    NvPclModuleSetting *output,
    NvBool PackageRegs)
{
    StateControllerContext *pContext;
    NvPclControlState TempState;
    NvPclControlState *pConfigState = NULL;

    if(!(hPclController) || !(hPclController->pPrivateContext)) {
        NvOsDebugPrintf("%s: Attempted to update controller without initialization\n", __func__);
        return NvError_BadParameter;
    }

    if(!(input) || !(output)) {
        NvOsDebugPrintf("%s: Both input and output setting configurations required\n", __func__);
        return NvError_BadParameter;
    }

    pContext = hPclController->pPrivateContext;

    if(PackageRegs) {
        pConfigState = &(pContext->ConfigurationState);
    } else {
        pConfigState = &TempState;
        NvOsMemset(pConfigState, 0, sizeof(NvPclControlState));
    }

    pConfigState->InputSettings = *input;
    NvOsMemcpy(&(pConfigState->OutputSettings),
                &(pConfigState->InputSettings),
                    sizeof(NvPclModuleSetting));

    *output = pConfigState->OutputSettings;

    return NvSuccess;
}

/**
 * Apply the last set of input settings to hardware.
 * An Update() call with PackageRegs=True is required to be done with the controller instance before
 * an Apply(); Update() is what prepares the configuration settings to be applied
 * @param hPclController the instance to apply.
 */
NvError NvPclStateControllerApply(NvPclControllerHandle hPclController) {
    StateControllerContext *pContext = NULL;
    NvError result;

    if(!(hPclController) || !(hPclController->pPrivateContext)) {
        NvOsDebugPrintf("%s: Attempted to apply controller without initialization\n", __func__);
        return NvError_BadParameter;
    }
    pContext = hPclController->pPrivateContext;

    // NvPclNvOdmApplyState reads and writes to ConfigurationState
    // (Writes to update OutputSettings if possible)
    result = NvPclNvOdmApplyState(hPclController);
    if(result != NvSuccess) {
        return result;
    }

    pContext->AppliedState.InputSettings = pContext->ConfigurationState.InputSettings;
    pContext->AppliedState.OutputSettings = pContext->ConfigurationState.OutputSettings;

    return NvSuccess;
}

/**
 * RunningInfo gives the currently applied settings corresponding to current hardware configuration
 * @param hPclController the instance to query
 * @param pInfo is already allocated memory to be filled with the applied settings
 * @param pInfoSize is size of data to be loaded into pInfo
 */
NvError
NvPclStateControllerRunningInfo(
    NvPclControllerHandle hPclController,
    NvPclRunningModuleInfo *pInfo,
    NvU32 *pInfoSize)
{
    NvError status = NvSuccess;
    NvPclModuleSetting *pWorkingSettings = NULL;
    NvPclCameraModule *pActiveModule = NULL;
    NvPclDriver *pDriver = NULL;
    NvU8 i = 0;

    if(!(hPclController) || !(hPclController->pPrivateContext)) {
        NvOsDebugPrintf("%s: Attempted to get state without initialization\n", __func__);
        return NvError_BadParameter;
    }

    NvPclReloadVolatileInfo(hPclController);

    pWorkingSettings = &(hPclController->pPrivateContext->AppliedState.OutputSettings);
    pActiveModule = hPclController->PlatformData.hFirstActiveModule;

    if(pInfo) {
        if(*pInfoSize != sizeof(NvPclRunningModuleInfo)) {
            NvOsDebugPrintf("%s: Size parameter was invalid\n", __func__);
            return NvError_BadParameter;
        }
        NvOsMemcpy(&pInfo->Settings, pWorkingSettings, sizeof(NvPclModuleSetting));
        for(i = 0; i < pActiveModule->NumSubDrivers; i++) {
            pDriver = &pActiveModule->SubDriver[i];
            switch(pDriver->Profile.Type) {
                case NvPclDriverType_Sensor:
                    NvOsMemcpy(&pInfo->SensorMode,
                        pDriver->Profile.pActiveObject,
                        sizeof(NvPclSensorObject));
                    break;
                case NvPclDriverType_Focuser:
                    NvOsMemcpy(&pInfo->FocuserMode,
                        pDriver->Profile.pActiveObject,
                        sizeof(NvPclFocuserObject));
                    break;
                case NvPclDriverType_Flash:
                    NvOsMemcpy(&pInfo->FlashList,
                        pDriver->Profile.ObjectList,
                        pDriver->Profile.NumObjects*sizeof(NvPclFlashObject));
                    break;
                default:
                    // No running info standard for others
                    break;
            }
        }
    }

    *pInfoSize = sizeof(NvPclRunningModuleInfo);

    return status;
}


/**
 * Close will clean up the controller instance
 * @param hPclController the instance to close
 */
void NvPclStateControllerClose(NvPclControllerHandle hPclController) {
    if(!hPclController) {
        NvOsDebugPrintf("%s: warning: NvPclHandle reference was empty\n", __func__);
        return;
    }

    NvPclCloseModuleDrivers(hPclController->PlatformData.hFirstActiveModule);

    if(hPclController->pPrivateContext->AppliedState.pHWConfigMemory) {
        NvOsFree(hPclController->pPrivateContext->AppliedState.pHWConfigMemory);
    }
    if(hPclController->pPrivateContext->ConfigurationState.pHWConfigMemory) {
        NvOsFree(hPclController->pPrivateContext->ConfigurationState.pHWConfigMemory);
    }
    if(hPclController->pPrivateContext) {
        if(hPclController->pPrivateContext->HWPackages.pSequence) {
            NvOsFree(hPclController->pPrivateContext->HWPackages.pSequence);
        }
        NvOsFree(hPclController->pPrivateContext);
    }

    NvOsFree(hPclController);

    return;
}



