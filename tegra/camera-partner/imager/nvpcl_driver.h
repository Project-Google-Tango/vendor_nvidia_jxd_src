/**
 * Copyright (c) 2013-2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef PCL_DRIVER_H
#define PCL_DRIVER_H


#include "nvodm_imager.h"
#include "nvpcl_hw_translator.h"
#include "nvpcl_driver.h"
#include "nvpcl_module_settings.h"

#if defined(__cplusplus)
extern "C"
{
#endif

typedef struct NvPclControlPackageRec *NvPclControlPackageHandle;
typedef struct NvPclDriverRec *NvPclDriverHandle;
typedef struct NvPclCameraModuleRec *NvPclCameraModuleHandle;

typedef struct NvOdmGUIDNameEntryRec
{
    const char *dev_name;
    NvU64 GUID;
} NvOdmGUIDNameEntry;

#define PCL_MAX_SIZE_FUSEID (16)
/**
 * A standardized container for all FuseIDs.
 */
typedef struct NvPclFuseIDRec
{
    NvU8 Data[PCL_MAX_SIZE_FUSEID];
    NvU8 NumBytes;
} NvPclFuseID;

/**
 * NvPclDriverType categorizes Pcl driver type
 */
typedef enum NvPclDriverType{
    NvPclDriverType_Sensor = 0,
    NvPclDriverType_Focuser,
    NvPclDriverType_Flash,
    NvPclDriverType_Rom,
    NvPclDriverType_Other,
    NvPclDriverType_Force32 = 0x7FFFFFFF
} NvPclDriverType;

/**
 * NvPclDriverProfile is for identifying static driver information
 * NvPclDrivers fill in the values according to their NvPcl<type>Object in their NvPclDriver_<type>.h file
 */
typedef struct NvPclDriverProfileRec
{
    // isOlNvOdmDriver is a flag for the system to recognize (inside NvPcl framework and camera/core)
    NvBool IsOldNvOdmDriver;
    // Data contains specific driver-type capabilities information
    NvPclDriverType Type;
    // NvPclHwBlob identifies how software recognizes the hardware instance (device name, address, etc)
    NvPclHwCameraSubModule HwDev;
    // FuseID is the unique identifier (a serial ID)
    NvPclFuseID FuseID;
    /**
     * ObjectList is defined by NvPcl<type>Object in their NvPclDriver_<type>.h file
     * Currently, defined as follows:
     *  Sensor  : List of varying sensor mode definitions
     *  Focuser : Unutilized; using numObjects of 1
     *  Flash   : List of LEDs available and their properties
     */
    NvU32 NumObjects;
    void *ObjectList;
    void *pActiveObject; // Use only for reporting active mode values.  RunningObject should match data types in ObjectList
} NvPclDriverProfile;

/**
 * NvPclDriver is a structure to define all device driver instances
 */
typedef struct NvPclDriverRec
{
    NvPclDriverProfile Profile;
    NvPclCameraModuleHandle hParentModule;
    /**
     * GetUpdateFun registers all Update() calls that the driver wants to do
     * when a new set input StateSettings are triggered.
     * @param pPclDriver is a handle for the driver instance
     * @param packages is the list to be registered into.  Sequence in functions are registered
     *              is the sequence in how they will be called.
     * @param numPackages is the number of packages/Update() registered in the driver
     */
    NvError (*pfnPclDriverGetUpdateFun)(struct NvPclDriverRec *pPclDriver, NvPclControlPackageHandle packages, NvU32 *numPackages);
    /**
     * GetVolatile provides the current settings of any volatile value.
     * Intended to be used when Controller is queried for the currently applied state (GetState())
     * @param pPclDriver is the driver instance
     * @param pControlSettings is the settings structure to override with any new volatile values.
     */
    NvError (*pfnPclDriverGetVolatile)(struct NvPclDriverRec *pPclDriver, NvPclModuleSetting *pSettings);
    /**
     * Initialize is the first function called to setup the driver profile and any private context.
     * NvPclDriver.profile members to be initialized are FuseID and Data. See NvPcl<type>Object for
     * details on what to fill Data with.
     * @param hModule is a handle to the module the driver is connected to
     * @param pPclDriver is the driver instance already allocated.
     */
    NvError (*pfnPclDriverInitialize)(NvPclCameraModuleHandle hModule, struct NvPclDriverRec *pPclDriver);
    /**
     * Close is used to clean up driver instance.
     * Free any allocated memory (ie. pPrivateContext)
     */
    void (*pfnPclDriverClose)(struct NvPclDriverRec *pPclDriver);

    // A holder for driver instance custom context...
    void *pPrivateContext;
} NvPclDriver;

/**
 * A generic Calibration structure definition
 */
typedef struct NvPclCalibrationRec
{
    NvU64 Size;
    void *pData;
} NvPclCalibration;

typedef enum {
    NvPclModulePlacement_Rear = 0,
    NvPclModulePlacement_Front,
    NvPclModulePlacement_Force32 = 0x7FFFFFFF
} NvPclModulePlacement;

/**
 * A Camera Module container.
 * TODO: investigate a module driver
 * TODO: consider making enumeration of drivers dynamic
 * TODO: rethink Module_ID vs sub_name
 */
typedef struct NvPclCameraModuleRec {
    NvBool Activated;
    // All other members are derived from HwDefinition
    NvPclHwCameraModule HwModuleDef;
    NvPclModulePlacement Direction;

    NvPclDriver SubDriver[MAX_NUM_CAM_SUBMODULES];
    NvU32 NumSubDrivers;

    /**
     * Any (currently non foreseeable) additional camera-module device should be added here
     * Multiple modules sharing a common device (ROM), should be safe
     * and programmed appropriately, distinguished by Driver.sub_name
     */

    NvPclCalibration CalibrationData;     // config .h file
    NvPclCalibration CalibrationOverride; // .isp file
#define MAX_FLASH_SIZE (1024*1) //1kB to be used with FactoryCalibration for backwards compat.
    NvPclCalibration FactoryCalibration;  // .bin file

    /**
     * NvOdmImager is here because it is only programmed to handle one instance of module
     * Future proofing of NvOdmImager is not supported, but only here for coding transition
     */
    NvOdmImagerHandle hImager;
    NvBool hImagerNeedFreeing;
} NvPclCameraModule;
#define PCL_MAX_NUM_MODULES (4)

#if defined(__cplusplus)
}
#endif




#endif  //PCL_DRIVER_H

