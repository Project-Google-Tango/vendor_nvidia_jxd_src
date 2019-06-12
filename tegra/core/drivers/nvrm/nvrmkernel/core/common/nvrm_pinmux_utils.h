/*
 * Copyright (c) 2007-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */
#ifndef NVRM_PINMUX_UTILS_H
#define NVRM_PINMUX_UTILS_H

/*
 * nvrm_pinmux_utils.h defines the pinmux macros to implement for the resource
 * manager.
 */

#include "nvcommon.h"
#include "nvrm_pinmux.h"
#include "nvrm_drf.h"
#include "nvassert.h"
#include "nvrm_hwintf.h"
#include "nvodm_modules.h"

// This is to disable trisate refcounting.
#define SKIP_TRISTATE_REFCNT 0

/*  The pin mux code supports run-time trace debugging of all updates to the
 *  pin mux & tristate registers by embedding strings (cast to NvU32s) into the
 *  control tables.
 */
#define NVRM_PINMUX_DEBUG_FLAG 0
#define NVRM_PINMUX_SET_OPCODE_SIZE_RANGE 3:1


#if NVRM_PINMUX_DEBUG_FLAG
NV_CT_ASSERT(sizeof(NvU32)==sizeof(const char*));
#endif

//  The extra strings bloat the size of Set/Unset opcodes
#define NVRM_PINMUX_SET_OPCODE_SIZE ((NVRM_PINMUX_DEBUG_FLAG) ? \
                                        NVRM_PINMUX_SET_OPCODE_SIZE_RANGE)

#ifdef __cplusplus
extern "C"
{
#endif  /* __cplusplus */

typedef enum {
    PinMuxConfig_OpcodeExtend = 0,
    PinMuxConfig_Set = 1,
    PinMuxConfig_Unset = 2,
    PinMuxConfig_BranchLink = 3,
} PinMuxConfigStates;

typedef enum {
    PinMuxOpcode_ConfigEnd = 0,
    PinMuxOpcode_ModuleDone = 1,
    PinMuxOpcode_SubroutinesDone = 2,
} PinMuxConfigExtendOpcodes;

//  for extended opcodes, this field is set with the extended opcode
#define MUX_ENTRY_0_OPCODE_EXTENSION_RANGE 3:2
//  The state for this entry
#define MUX_ENTRY_0_STATE_RANGE 1:0

#define MAX_NESTING_DEPTH 4

/*  This macro is used for opcode entries in the tables */
#define PIN_MUX_OPCODE(_OP_) \
    (NV_DRF_NUM(MUX,ENTRY,STATE,PinMuxConfig_OpcodeExtend) | \
     NV_DRF_NUM(MUX,ENTRY,OPCODE_EXTENSION,(_OP_)))

/*  This is a dummy entry in the array which indicates that all setting/unsetting for
 *  a configuration is complete. */
#define CONFIGEND() PIN_MUX_OPCODE(PinMuxOpcode_ConfigEnd)

/*  This is a dummy entry in the array which indicates that the last configuration
 *  for the module instance has been passed. */
#define MODULEDONE()  PIN_MUX_OPCODE(PinMuxOpcode_ModuleDone)

/*  This is a dummy entry in the array which indicates that all "extra" configurations
 *  used by sub-routines have been passed. */
#define SUBROUTINESDONE() PIN_MUX_OPCODE(PinMuxOpcode_SubroutinesDone)

/*  This macro is used to insert a branch-and-link from one configuration to another */
#define BRANCH(_ADDR_) \
     (NV_DRF_NUM(MUX,ENTRY,STATE,PinMuxConfig_BranchLink) | \
      NV_DRF_NUM(MUX,ENTRY,BRANCH_ADDRESS,(_ADDR_)))

/** NvRmSetGpioTristate will either enable or disable the tristate for GPIO ports.
  * RM client gpio should only call NvRmSetGpioTristate,
  * which will program the tristate correctly based pins of the particular port.
  *
  *@param hDevice The RM instance
  *@param Port The GPIO port to set
  *@param Pin The Pinnumber  of the port to set.
  *@param EnableTristate NV_TRUE will tristate the specified pins, NV_FALSE will un-tristate
  */

void NvRmSetGpioTristate(
    NvRmDeviceHandle hDevice,
    NvU32 Port,
    NvU32 Pin,
    NvBool EnableTristate);

/** NvRmPrivRmModuleToOdmModule will perform the mapping of RM modules to
 *  ODM modules and instances, using the chip-specific mapping wherever
 *  necessary */
NvU32 NvRmPrivRmModuleToOdmModule(
    NvU32 RmModule,
    NvOdmIoModule *pOdmModules,
    NvU32 *pOdmInstances);

NvBool NvRmGetPinForGpio(
    NvRmDeviceHandle hDevice,
    NvU32 Port,
    NvU32 Pin,
    NvU32* pMapping);

NvBool NvRmPrivChipSpecificRmModuleToOdmModule(
    NvRmModuleID ModuldID,
    NvOdmIoModule* pOdmModules,
    NvU32* pOdmInstances,
    NvU32 *pCnt);

void NvRmPrivInitTrisateRefCount(NvRmDeviceHandle hDevice);

const NvU32*
NvRmPrivFindConfigStart(
    const NvU32* Instance,
    NvU32 Config,
    NvU32 EndMarker);

void
NvRmPrivSetGpioTristate(
    NvRmDeviceHandle hDevice,
    NvU32 Port,
    NvU32 Pin,
    NvBool EnableTristate);

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif // NVRM_PINMUX_UTILS_H


