/*
 * Copyright (c) 2007-2011 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef NVRM_MODULE_PRIVATE_H
#define NVRM_MODULE_PRIVATE_H

#include "nvcommon.h"
#include "nvrm_init.h"
#include "nvrm_relocation_table.h"
#include "nvrm_moduleids.h"

#ifdef __cplusplus
extern "C"
{
#endif  /* __cplusplus */

typedef struct NvRmModuleTableRec
{
    NvRmModule Modules[NvRmPrivModuleID_Num];
    NvRmModuleInstance *ModInst;
    NvRmModuleInstance *LastModInst;
    NvU32 NumModuleInstances;
    NvRmIrqMap IrqMap;
} NvRmModuleTable;


typedef struct NvRmModuleIdRec
{
    NvRmModuleID ModuleId;
} NvRmModuleId;

/**
 * Initialize the module info via the relocation table.
 *
 * @param mod_table The module table
 * @param reloc_table The relocation table
 * @param modid The module id conversion function
 */
NvError
NvRmPrivModuleInit(
    NvRmModuleTable *mod_table,
    NvU32 *reloc_table);

void
NvRmPrivModuleDeinit(
    NvRmModuleTable *mod_table );

NvRmModuleTable *
NvRmPrivGetModuleTable(
    NvRmDeviceHandle hDevice );

NvBool
NvRmModuleIsMemory(
    NvRmDeviceHandle hDevHandle,
    NvRmModuleID ModId);

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif // NVRM_MODULE_PRIVATE_H
