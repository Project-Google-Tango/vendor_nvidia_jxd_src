/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_nvrm_chip_H
#define INCLUDED_nvrm_chip_H


#if defined(__cplusplus)
extern "C"
{
#endif

#include "nvrm_init.h"

typedef enum
{
    NvRmChipPlatform_Silicon,
    NvRmChipPlatform_Simulation,
    /* NvRmChipPlatform_FPGA detected as Silicon */
    /* NvRmChipPlatform_QT detected as Silicon */
} NvRmChipPlatform;

typedef enum
{
    NvRmChipDisplayVersion_1,
    NvRmChipDisplayVersion_2,
    NvRmChipDisplayVersion_3,
    NvRmChipDisplayVersion_4
} NvRmChipDisplayVersion;

typedef enum
{
    NvRmChipVICVersion_3,
} NvRmChipVICVersion;

typedef enum
{
    /* System capabilities */
    NvRmChipCapability_System_SMMU                  = 0x00000001,
    NvRmChipCapability_System_VPR                   = 0x00000002,
    NvRmChipCapability_System_TiledLayout           = 0x00000003,
    NvRmChipCapability_System_BlocklinearLayout     = 0x00000010,

    /* GPU capabilities */
    NvRmChipCapability_GPU_TiledLinearTextures      = 0x00000101,
    NvRmChipCapability_GPU_NumPixelPipes            = 0x00000102,
    NvRmChipCapability_GPU_NumALUsPerPixelPipe      = 0x00000103,

    /* Display capabilities */
    NvRmChipCapability_Display_Rotation             = 0x00000201,
    NvRmChipCapability_Display_Version              = 0x00000202,

    /* VIC capabilities */
    NvRmChipCapability_VIC_Version                  = 0x00000301,

    /* Bug bits. Defined as (0x80000000 | Bug Id) to make them mergeable
     * between branches.
     */
    NvRmChipCapability_Bug_1060067                  = 0x80000000 | 1060067,
} NvRmChipCapability;

NvError NvRmChipGetCapabilityBool(
    NvRmChipCapability Capability,
    NvBool *Out);

NvError NvRmChipGetCapabilityU32(
    NvRmChipCapability Capability,
    NvU32 *Out);

NvRmChipPlatform NvRmChipGetPlatform(void);

#if defined(__cplusplus)
}
#endif

#endif
