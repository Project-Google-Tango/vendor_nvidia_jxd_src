/**
 * Copyright (c) 2013-2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef PCL_CONTROLLER_H
#define PCL_CONTROLLER_H

#if defined(__cplusplus)
extern "C"
{
#endif

#include "nvos.h"
#include "nvcamera_error.h"
/**
 *  NvPcl includes
 */
#include "nvpcl_driver.h"
#include "nvpcl_driver_sensor.h"
#include "nvpcl_driver_focuser.h"
#include "nvpcl_driver_flash.h"
#include "nvpcl_module_settings.h"

#define  PCL_MAX_NUM_PKG (128)

typedef struct NvPclControllerRec *NvPclControllerHandle;
typedef struct NvPclPlatformDataRec *NvPclPlatformDataHandle;

typedef struct NvPclPlatformDataRec
{
    NvPclCameraModule ModuleList[PCL_MAX_NUM_MODULES];
    NvPclCameraModuleHandle hFirstActiveModule;
    NvU8 NumModules;
}NvPclPlatformData;

/**
 * Container holding programmable settings relative to a state.
 * A state holds the original desired (input) settings, paired with
 * a corresponding actual (output) settings.  Output settings are
 * intended to expose device granularity/resolution and limitations
 * calculated by individual device drivers. The hw register values
 * correspond to the state's output settings.
 */
typedef struct NvPclControlStateRec
{
    NvPclModuleSetting InputSettings;       // The original desired configuration
    NvPclModuleSetting OutputSettings;     // What ODM says it will do post processing
    NvPclHwReg *pHWConfigMemory;   // HWReg values of WorkingSettings member
} NvPclControlState;

/**
 * A single Control Package configuration.
 */
typedef struct NvPclControlPackageRec
{
    /**
     * Dirty is driven by the driver instance hDriver
     * The controller will reference Dirty to determine
     * if the hw registers in HwBlob need to be written
     * out to hardware
     */
    NvBool Dirty;
    /**
     * HwBlob is a packet that contains the hw destination device
     * (sensor, focuser, flash), and a register list. Actual register
     * data can be found in HwBlob.regList.regdata_p.
     * By default, the destination will be its own driver instance.
     * This also opens up the possibility if cross-device communication
     * is required (sensor driver can write a command to focuser device),
     * but this should be leveraged sparingly for an extreme hack
     * situation, and a different driver design should be considered before
     * using this.
     */
    NvPclHwBlob HwBlob;
    /**
     * A handle to the driver instance.  Used for potential verbosity in
     * NvPcl Controller, but driver Update functions can grab their instance
     * context from here as well.
     */
    NvPclDriverHandle hDriver;
    /**
     * Every segment of register memory is controlled by a driver Update
     * function. The goal of an Update function is to expose the drivers
     * ability to best fulfill the desired settings.  The driver fills in (requested->InputSettings).
     * its actual capable settings (requested->OutputSettings) that would
     * result from the desired settings (requested->InputSettings).
     */
    NvError (*Update)(const NvPclControlState *applied,
                    NvPclControlState *requested,
                    struct NvPclControlPackageRec *pPackage);
    /**
     * Want to ensure drivers don't touch configuration hw memory if register packaging is
     * Not required.  If we did not care about drivers touching configuration memory,
     * we could pass in a NULL applied state, since the applied state does not reveal more
     * about a drivers capability, but only assists in marking a package Dirty.
     * Drivers should check if HwBlob exists to determine if it should fill in register values.
     * (An alternative implementation is to have another memory instance for updates)
     */
} NvPclControlPackage;

/**
 * A container to describe a list of packages.
 * Only one instance of PackageList to be running per Controller.
 */
typedef struct NvPclPackageListRec
{
    NvU32 numPackages;
    NvU32 numRegisters;
    NvPclControlPackage *pSequence;
} NvPclPackageList;

/**
 * State Controller's private context.
 */
typedef struct NvPclStateControllerContextRec
{
    NvPclControlState ConfigurationState;
    NvPclControlState AppliedState;
    NvPclPackageList HWPackages;          // Describes how State.HWConfigMemory is administrated
} StateControllerContext;

typedef struct NvPclControllerRec
{
    NvPclPlatformData PlatformData;
    StateControllerContext *pPrivateContext;
} NvPclController;

/**
 * Open is the initializing function that will create a new Controller instance based
 * on the ImagerGUID provided.
 * @param phPclController is the handle where a new instance will be assigned. Must be
 *                          passed in NULL.
 * @param ImagerGUID is the camera module setup to be selected.
 */
NvError NvPclStateControllerOpen(NvU64 ImagerGUID, NvPclControllerHandle *phPclController, NvOdmImagerHandle hImager);

/**
 * GetPlatformData is a querying function for initial, static parameters.
 * @param hPclController is the instance to query
 * @param pData is a pointer to where the information will be stored.
 *       it should be pointed to already allocated memory
 */
NvError
NvPclStateControllerGetPlatformData(
    const NvPclControllerHandle hPclController,
    NvPclPlatformData **phData);

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
    NvBool PackageRegs);

/**
 * Apply the last set of input settings to hardware.
 * An Update() call with PackageRegs=True is required to be done with the controller instance before
 * an Apply(); Update() is what prepares the configuration settings to be applied
 * @param hPclController the instance to apply.
 */
NvError NvPclStateControllerApply(NvPclControllerHandle hPclController);

/**
 * RunningInfo gives the running device mode profiles with active hardware settings
 * @param hPclController the instance to query
 * @param pInfo is a pointer to where the information will be stored.  If NULL, no information
 *              will be stored; otherwise, it should be pointed to already allocated memory
 * @param pInfoSize is the amount of data that is being handled in byte size.
 */
NvError
NvPclStateControllerRunningInfo(
    NvPclControllerHandle hPclController,
    NvPclRunningModuleInfo *pInfo,
    NvU32 *pInfoSize);

/**
 * Close will clean up the controller instance
 * @param hPclController the instance to close
 */
void NvPclStateControllerClose(NvPclControllerHandle hPclController);

#if defined(__cplusplus)
}
#endif

#endif  //PCL_CONTROLLER_H

