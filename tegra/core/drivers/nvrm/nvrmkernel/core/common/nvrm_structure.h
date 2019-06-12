/*
 * Copyright (c) 2008-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_NVRM_STRUCTURE_H
#define INCLUDED_NVRM_STRUCTURE_H

/*
 * nvrm_structure.h defines all of the internal data structures for the
 * resource manager which are chip independent.
 *
 * Don't add chip specific stuff to this file.
 */

#include "nvcommon.h"
#include "nvos.h"
#include "nvrm_module.h"
#include "nvrm_module_private.h"
#include "nvrm_chipid.h"
#include "nvrm_interrupt.h"
#include "nvrm_memmgr.h"
#include "nvrm_pinmux.h"
#include "nvrm_rmctrace.h"
#include "nvrm_configuration.h"
#include "nvrm_relocation_table.h"
#include "nvrm_moduleids.h"

#ifdef __cplusplus
extern "C"
{
#endif  /* __cplusplus */

typedef struct RmConfigurationVariables_t
{
    /* RMC Trace file name */
    char RMCTraceFileName[NVRM_CFG_MAXLEN];
} RmConfigurationVariables;

/* memory pool information */
typedef struct RmMemoryPool_t
{
    NvU32 base;
    NvU32 size;
} RmMemoryPool;

/* The state for the resource manager */
typedef struct NvRmDeviceRec
{
    RmConfigurationVariables cfg;
    NvRmRmcFile rmc;
    NvBool rmc_enable;
    NvOsMutexHandle mutex;
    //  FIXME:  this is hardcoded to the number of tristate registers in AP15.
    NvS16 TristateRefCount[4 * sizeof(NvU32)*8];
    NvU32 refcount;

    NvOsMutexHandle MemMgrMutex;
    NvOsMutexHandle PinMuxMutex;
    NvOsMutexHandle CarMutex;   /* r-m-w top level CAR registers mutex */

    /* chip id */
    NvRmChipId ChipId;

    /* module instances and module index table */
    NvRmModuleTable ModuleTable;

    RmMemoryPool ExtMemoryInfo;
    RmMemoryPool IramMemoryInfo;
    RmMemoryPool GartMemoryInfo;

    NvU16 MaxIrqs;

    const NvU32  ***PinMuxTable;
    // FIXME: get rid of all the various Init and Open functions in favor
    // of a sane state machine for system boot/initialization
    NvBool bPreInit;
    NvBool bBasicInit;
} NvRmDevice;

// FIXME: This macro should be comming from the relocation table.
#define NVRM_MAX_INSTANCES 32

/**
 * Sub-contoller interrupt decoder description forward reference.
 */
typedef struct NvRmIntrDecoderRec *NvRmIntrDecoderHandle;

/**
 * Attributes of the Interrupt sub-decoders.
 */
typedef struct NvRmIntrDecoderRec
{
    NvRmModuleID ModuleID;

    //  Number of IRQs owned by this sub-controller.
    //  This value is same for all the instances of the controller.
    NvU32 SubIrqCount;

    // Number of instance for this sub-decoder
    NvU32 NumberOfInstances;
    
    // Main controller IRQ.
    NvU16 MainIrq[NVRM_MAX_INSTANCES];

    // First IRQ owned by this sub-controller.
    NvU16 SubIrqFirst[NVRM_MAX_INSTANCES];

    // Last IRQ owned by this sub-controller.
    NvU16 SubIrqLast[NVRM_MAX_INSTANCES];

} NvRmIntrDecoder;

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif // INCLUDED_NVRM_STRUCTURE_H
