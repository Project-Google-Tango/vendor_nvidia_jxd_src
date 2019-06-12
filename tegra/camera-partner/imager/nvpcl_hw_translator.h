/**
 * Copyright (c) 2012-2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


#ifndef PCL_HW_TRANSLATOR_H
#define PCL_HW_TRANSLATOR_H

#include "nvos.h"
// This file will differ for different OS platforms

#define NVPCL_STR_NAME_MAX (26)

/**
 * Enum defining how the package should be handled (device hw abstracted)
 */
typedef enum NvPclPackageType{
    NvPclPackageType_LAUNCHASYNC = 0,     // Write packages with HOLD out to hardware ASAP, in addition to any information in this package
    NvPclPackageType_LAUNCHSYNC,      // FUTURE TBD? If we decide to synch in kernel itself
    NvPclPackageType_HOLD,            // Hold package information, until a LAUNCH command to write out to hardware
    NvPclPackageType_CLEAR,           // Clears current data of any data in Hold; a reset
    NvPclPackageType_BEGIN_VENDOR_EXTENSIONS = 0x10000000,
    NvPclPackageType_FORCE32 = 0x7FFFFFFF
} NvPclPackageType;

/**
 * Enum defining how memory (register) values should be interpreted and handled
 */
typedef enum NvPclHwInstructionType{
    NvPclHwInstructionType_NOOP = 0,
    NvPclHwInstructionType_I2C,
    NvPclHwInstructionType_GPIO,
    NvPclHwInstructionType_SPI,
    NvPclHwInstructionType_USLEEP,
    NvPclHwInstructionType_VREG,
    NvPclHwInstructionType_CSI3,    // TBD
    NvPclHwInstructionType_BEGIN_VENDOR_EXTENSIONS = 0x10000000,
    NvPclHwInstructionType_FORCE32 = 0x7FFFFFFF,
} NvPclHwInstructionType;

/**
 * Struct container for a register, or memory location
 */
typedef struct NvPclHwReg
{
    NvPclHwInstructionType type;
    NvU16 regaddr;
    NvU16 regval;
} NvPclHwReg;

/**
 * Struct to define a list of registers grouped together sequentially
 */
typedef struct NvPclHwRegList
{
    NvU16 regdata_length;
    NvU8 regaddr_bits;
    NvU8 regval_bits;
    NvPclHwReg *regdata_p;
} NvPclHwRegList;

typedef struct NvPclHwCameraSubModuleRec {
    char Name[NVPCL_STR_NAME_MAX];
    void *hDev;
} NvPclHwCameraSubModule;

#define MAX_NUM_CALIBRATIONS (16)
#define NVPCL_GENERICMODULENAME ("GenericModuleName")
#define PCL_STR_LIST_END ("PCL_STRING_LIST_END")

/**
 * Struct to define a submodule hardware device instance
 */
typedef struct NvPclHwBlobRec
{
    NvPclHwCameraSubModule dev;
    NvPclHwRegList regList;
} NvPclHwBlob;

typedef struct NvPclHwStateInstructionRec {
    NvPclHwInstructionType type;
    char *name;
    NvU16 value;
} NvPclHwStateInstruction;

#define MAX_NUM_CAM_SUBMODULES (8)
typedef struct NvPclHwCameraModuleRec {
    char Name[NVPCL_STR_NAME_MAX];  // String name for module identification
    NvPclHwCameraSubModule HwCamSubModule[MAX_NUM_CAM_SUBMODULES];

    const char * CalibrationData;                               // Included config .h file
    const char * CalibrationOverride[MAX_NUM_CALIBRATIONS];     // Path to .isp file
    const char * FactoryCalibration[MAX_NUM_CALIBRATIONS];      // Path to .bin file
} NvPclHwCameraModule;


/**
 * NvPclKernelWrite is a generic interface for kernel writing interactions.
 * @param controlType holds protocol or policy info for pData.
 * @param pBlobs pointer to list of hw blobs - we may send blobs to represent each sensor, focuser, and flash in one command
 * @param numBlobs number of NvPclHwBlob passed in pBlobs
 * @returns error if kernel fail or couldn't construct a message to kernel (unsupported or invalid); otherwise success?
 */
NvError NvPclHwWrite(NvPclPackageType controlType, NvPclHwBlob *pBlobs, NvU32 numBlobs);


/**
 * NvPclKernelRead is a generic interface for kernel reading interactions.
 * @param controlType holds protocol or policy info for pData.
 * @param pBlobs pointer to list of hw blobs to be read into
 * @param numData expected number of NvPcl_ControlReg passed back to pData
 * @returns error if kernel fail or couldn't construct a message to kernel (unsupported or invalid); otherwise success?
 */
NvError NvPclHwRead(NvPclPackageType controlType, NvPclHwBlob *pBlobs, NvU32 *pNumBlobs);


/**
 * NvPclKernelGetProfile provides a list of devices found in the kernel.  It should identify and theoretically enable NvPclController to decide which NvPclDriver should be used (ie. AR0833, IMX135, etc)
 * @param ModuleList if not NULL, memory to be filled with strings; should have NumModules allocated.  If a module member device is not used on camera, use NULL value in slot if slot is <= NumModules.
 * @param pNumModules number of camera modules; if NULL, error
 * @param isVirtual boolean to state intent of a virtual device
 * @returns error if kernel fail or couldn't construct a message to kernel (unsupported or invalid); otherwise success.  (We may want to try to utilize DeviceTree in future)
 */
NvError NvPclHwGetModuleList(NvPclHwCameraModule *pModules, NvU32 *pNumModules, NvBool isVirtual);

NvError NvPclHwInitializeModule(NvPclHwCameraModule *pModule);

void NvPclHwPrintCameraSubModule(NvU32 index, NvPclHwCameraSubModule dev);
void NvPclHwPrintModuleDefinition(NvPclHwCameraModule hwModule);
void NvPclPrintToStrEnd(const char * typeStr, const char *strArray[]);

#endif /* PCL_HW_TRANSLATOR_H */
