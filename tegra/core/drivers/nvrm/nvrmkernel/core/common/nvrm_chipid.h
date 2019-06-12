/*
 * Copyright (c) 2007, 2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_NVRM_CHIPID_H
#define INCLUDED_NVRM_CHIPID_H

#include "nvcommon.h"
#include "nvrm_init.h"

#ifdef __cplusplus
extern "C"
{
#endif  /* __cplusplus */

/* Chip Id */
typedef enum
{
    NvRmChipFamily_Gpu = 0,
    NvRmChipFamily_Handheld = 1,
    NvRmChipFamily_BrChips = 2,
    NvRmChipFamily_Crush = 3,
    NvRmChipFamily_Mcp = 4,
    NvRmChipFamily_Ck = 5,
    NvRmChipFamily_Vaio = 6,
    NvRmChipFamily_HandheldSoc = 7,

    NvRmChipFamily_Force32 = 0x7FFFFFFF,
} NvRmChipFamily;

typedef enum
{
    NvRmCaps_HasFalconInterruptController = 0,
    NvRmCaps_Has128bitInterruptSerializer,
    NvRmCaps_Num,
    NvRmCaps_Force32 = 0x7FFFFFFF,
}  NvRmCaps;

typedef struct NvRmChipIdRec
{
    NvU16 Id;
    NvU8 Major;
    NvU8 Minor;
    NvU16 SKU;

    /* the following only apply for emulation -- Major will be 0 and
     * Minor is either 0 for quickturn or 1 for fpga
     */
    NvU16 Netlist;
    NvU16 Patch;

    /* List of features and bug WARs */
    NvU32 Flags[(NvRmCaps_Num+31)/32];

    /* Chip-private feature string */
    char *Private;
} NvRmChipId;

#define NVRM_IS_CAP_SET(h, bit) \
    (((h)->ChipId.Flags)[(bit) >> 5] & (1 << ((bit) & 31)))
#define NVRM_CAP_SET(h, bit) \
    (((h)->ChipId.Flags)[(bit) >> 5] |= (1U << ((bit) & 31U)))
#define NVRM_CAP_CLEAR(h, bit) \
    (((h)->ChipId.Flags)[(bit) >> 5] &= ~(1U << ((bit) & 31U)))

/**
 * Gets the chip id.
 *
 * @param hDevice The RM instance
 */
NvRmChipId *
NvRmPrivGetChipId( NvRmDeviceHandle hDevice );

#define NVRM_AP20_ID 0x20
#define NVRM_T30_ID  0x30
#define NVRM_T114_ID 0x35
#define NVRM_T148_ID 0x14
#define NVRM_T124_ID 0x40

/* Contains only Id and Revision info */
typedef struct ChipIdRec
{
    NvU32 Id;
    NvU32 Revision;
} NvRmChipIdLimited;

/**
 * Gets the NvRmChipIdLimited structure.
 *
 */
NvRmChipIdLimited
NvRmPrivGetChipIdLimited( void );

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif // INCLUDED_NVRM_CHIPID_H
