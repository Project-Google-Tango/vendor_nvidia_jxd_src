/*
 * Copyright (c) 2007 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_NVRM_CONFIGURATION_H
#define INCLUDED_NVRM_CONFIGURATION_H

#include "nvcommon.h"
#include "nverror.h"

/**
 * The RM configuration variables are represented by two structures:
 * a configuration map, which lists all of the variables, their default
 * values and types, and a struct of strings, which holds the runtime value of
 * the variables.  The map holds the index into the runtime structure.
 *
 */

/**
 * The configuration varible type.
 */
typedef enum
{
    /* String should be parsed as a decimal */
    NvRmCfgType_Decimal = 0,

    /* String should be parsed as a hexadecimal */
    NvRmCfgType_Hex = 1,

    /* String should be parsed as a character */
    NvRmCfgType_Char = 2,

    /* String used as-is. */
    NvRmCfgType_String = 3,
} NvRmCfgType;

/**
 * The configuration map (all possible variables).  The map must be
 * null terminated.  Each Rm instance (for each chip) can/will have
 * different configuration maps.
 */
typedef struct NvRmCfgMap_t
{  
    const char *name;
    NvRmCfgType type; 
    void *initial; /* default value of the variable */
    void *offset; /* the index into the string structure */
} NvRmCfgMap;

/* helper macro for generating the offset for the map */
#define STRUCT_OFFSET( s, e )       (void *)(&(((s*)0)->e))

/* maximum size of a configuration variable */
#define NVRM_CFG_MAXLEN NVOS_PATH_MAX

/**
 * get the default configuration variables.
 *
 * @param map The configuration map
 * @param cfg The configuration runtime values
 */
NvError
NvRmPrivGetDefaultCfg( NvRmCfgMap *map, void *cfg );

/**
 * get requested configuration.
 *
 * @param map The configuration map
 * @param cfg The configuration runtime values
 *
 * Note: 'cfg' should have already been initialized with
 * NvRmPrivGetDefaultCfg()  before calling this.
 */
NvError
NvRmPrivReadCfgVars( NvRmCfgMap *map, void *cfg );

#endif
