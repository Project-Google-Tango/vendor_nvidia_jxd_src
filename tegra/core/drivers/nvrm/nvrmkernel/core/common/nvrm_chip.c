/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvrm_chip.h"
#include "nvrm_chip_private.h"
#include "nvrm_chipid.h"
#include "nvrm_hardware_access.h"
#include "nvrm_module.h"
#include "nvassert.h"

/* Minimum revision 0 means any revision will match */
#define ANY_REVISION 0

/* Platform masks */
#define SILICON     (1 << NvRmChipPlatform_Silicon)
#define SIMULATION  (1 << NvRmChipPlatform_Simulation)
#define ANY_PLATFORM 0xFFFFFFFF

typedef struct NvRmPrivChipCapsRec
{
    struct
    {
        NvU32 SMMU;
        NvU32 VPR;
        NvU32 TiledLayout;
        NvU32 BlocklinearLayout;
    } System;
    struct
    {
        NvU32 TiledLinearTextures;
        NvU32 NumPixelPipes;
        NvU32 NumALUsPerPixelPipe;
    } GPU;
    struct
    {
        NvU32 Rotation;
        NvU32 Version;
    } Display;
    struct
    {
        NvU32 Id1060067 : 1;
    } Bug;
    struct
    {
        NvU32 Version;
    } VIC;
} NvRmPrivChipCaps;

typedef struct NvRmPrivChipInfoRec
{
    NvRmChipIdLimited Chip;
    NvRmChipPlatform Platform;
    const char *Name;
    const char *MarketingName;
    NvRmPrivChipCaps Capabilities;
} NvRmPrivChipInfo;

typedef struct NvRmPrivChipSpecRec
{
    NvU32 Id;
    NvU32 MinimumRevision;
    NvU32 PlatformMask;
    void (*GetInfo)(NvRmPrivChipInfo*);
} NvRmPrivChipSpec;


/*
 * Chip-specific information encoded in this file
 */

static void GetInfo_AP20(NvRmPrivChipInfo *chip)
{
    chip->Name = "T20";
    chip->MarketingName = "Tegra 2";
    chip->Capabilities.System.TiledLayout = 1;
    chip->Capabilities.Display.Version = NvRmChipDisplayVersion_1;
    chip->Capabilities.Bug.Id1060067 = 1;
}

static void GetInfo_T30(NvRmPrivChipInfo *chip)
{
    chip->Name = "T30";
    chip->MarketingName = "Tegra 3",
    chip->Capabilities.System.SMMU = 1;
    chip->Capabilities.System.TiledLayout = 1;
    chip->Capabilities.Display.Version = NvRmChipDisplayVersion_1;
    chip->Capabilities.Bug.Id1060067 = 1;
}

static void GetInfo_T114(NvRmPrivChipInfo *chip)
{
    chip->Name = "T114";
    chip->MarketingName = "Tegra 4",
    chip->Capabilities.System.SMMU = 1;
    chip->Capabilities.System.VPR = 1;
    chip->Capabilities.System.TiledLayout = 1;
    chip->Capabilities.GPU.TiledLinearTextures = 1;
    chip->Capabilities.Display.Rotation = 1;
    chip->Capabilities.Display.Version = NvRmChipDisplayVersion_2;
}

static void GetInfo_T148(NvRmPrivChipInfo *chip)
{
    chip->Name = "T148";
    chip->MarketingName = "Tegra 4",
    chip->Capabilities.System.SMMU = 1;
    chip->Capabilities.System.VPR = 1;
    chip->Capabilities.System.TiledLayout = 1;
    chip->Capabilities.GPU.TiledLinearTextures = 1;
    chip->Capabilities.Display.Version = NvRmChipDisplayVersion_3;
}

static void GetInfo_T124(NvRmPrivChipInfo *chip)
{
    chip->Name = "T124";
    chip->MarketingName = "Tegra 5",
    chip->Capabilities.System.SMMU = 1;
    chip->Capabilities.System.VPR = 1;
    chip->Capabilities.System.BlocklinearLayout = 1;
    chip->Capabilities.Display.Rotation = 1;
    chip->Capabilities.Display.Version = NvRmChipDisplayVersion_4;
    chip->Capabilities.VIC.Version = NvRmChipVICVersion_3;
}



static void GetInfo_Common(NvRmDeviceHandle hRm, NvRmPrivChipInfo *chip)
{
    NvRmPrivGpuInfo gpuInfo;
    NvOsMemset(&gpuInfo, 0, sizeof(gpuInfo));

    NvRmPrivGpuGetInfo(hRm, &gpuInfo);
    chip->Capabilities.GPU.NumPixelPipes = gpuInfo.NumPixelPipes;
    chip->Capabilities.GPU.NumALUsPerPixelPipe = gpuInfo.NumALUsPerPixelPipe;
}

/*
 * Chip detection:
 *
 * An entry in this table is considered to match the current chip if
 *  1) Chip ID in the table matches exactly the chip id of the system
 *  2) Minimum revision in the table is <= the chip revision of the system
 *  3) Platform mask in the table has the system's platform bit set
 *
 * All entries in the table are searched and the last match is chosen.
 * Thus you should add larger revision numbers after smaller ones and
 * stricter platform masks after looser ones.
 */
static NvRmPrivChipSpec s_ChipSpec[] = {
    { NVRM_AP20_ID, ANY_REVISION, ANY_PLATFORM, GetInfo_AP20 },
    { NVRM_T30_ID,  ANY_REVISION, ANY_PLATFORM, GetInfo_T30  },
    { NVRM_T114_ID, ANY_REVISION, ANY_PLATFORM, GetInfo_T114 },
    { NVRM_T148_ID, ANY_REVISION, ANY_PLATFORM, GetInfo_T148 },
    { NVRM_T124_ID, ANY_REVISION, ANY_PLATFORM, GetInfo_T124 },
};

static NvRmChipPlatform GetPlatform(void)
{
    /* Detecting FPGA/QT not supported yet */
    return NvRmChipPlatform_Silicon;
}

static const NvRmPrivChipInfo* GetChipInfo(void)
{
    NvU32 i;
    NvRmChipIdLimited chipId;
    NvRmChipPlatform platform;
    NvRmPrivChipSpec *match = NULL;
    NvRmDeviceHandle hRm;
    NvError err;
    static NvRmPrivChipInfo s_chipInfo;
    static NvBool s_chipInfoInitialized = NV_FALSE;

    if (s_chipInfoInitialized)
    {
        return &s_chipInfo;
    }

    chipId = NvRmPrivGetChipIdLimited();
    platform = GetPlatform();
    for (i = 0; i < NV_ARRAY_SIZE(s_ChipSpec); i++)
    {
        NvRmPrivChipSpec *s = &s_ChipSpec[i];
        if ((s->Id == chipId.Id) &&
            (s->MinimumRevision <= chipId.Revision) &&
            (s->PlatformMask & (1 << platform)))
        {
            match = s;
        }
    }

    if (!match)
    {
        NvOsDebugPrintf("No matching chip spec found for chip "
            "Id=%d, Revision=%d, Platform=%d\n",
            chipId.Id, chipId.Revision, platform);
        return NULL;
    }

    NvOsMemset(&s_chipInfo, 0, sizeof(s_chipInfo));
    s_chipInfo.Chip = chipId;
    s_chipInfo.Platform = platform;

    err = NvRmOpen(&hRm, 0);
    if (err != NvSuccess)
    {
        NvOsDebugPrintf("NvRmOpen failed: %d\n", err);
        return NULL;
    }

    // Common capability detection for all chips (e.g. from kernel)
    GetInfo_Common(hRm, &s_chipInfo);

    // Chip-specific capabilities encoded in this file
    match->GetInfo(&s_chipInfo);

    NvRmClose(hRm);

    s_chipInfoInitialized = NV_TRUE;
    return &s_chipInfo;
}


/*
 * Public API
 */

NvError NvRmChipGetCapabilityBool(NvRmChipCapability Capability, NvBool *Out)
{
    const NvRmPrivChipInfo *chip = GetChipInfo();
    NV_ASSERT(chip);

#define CAP(category, cap) \
    case NvRmChipCapability_##category##_##cap: \
        *Out = !!chip->Capabilities.category.cap; break

    switch(Capability)
    {
        CAP(System, SMMU);
        CAP(System, VPR);
        CAP(System, TiledLayout);
        CAP(System, BlocklinearLayout);
        CAP(GPU, TiledLinearTextures);
        CAP(Display, Rotation);
        case NvRmChipCapability_Bug_1060067:
            *Out = !!chip->Capabilities.Bug.Id1060067; break;
        default:
            return NvError_BadValue;
    }

#undef CAP

    return NvSuccess;
}

NvError NvRmChipGetCapabilityU32(NvRmChipCapability Capability, NvU32 *Out)
{
    const NvRmPrivChipInfo *chip = GetChipInfo();
    NV_ASSERT(chip);

#define CAP(category, cap) \
    case NvRmChipCapability_##category##_##cap: \
        *Out = chip->Capabilities.category.cap; break

    switch(Capability)
    {
        CAP(GPU, NumPixelPipes);
        CAP(GPU, NumALUsPerPixelPipe);
        CAP(Display, Version);
        CAP(VIC, Version);
        default:
            return NvError_BadValue;
    }

#undef CAP

    return NvSuccess;
}

NvRmChipPlatform NvRmChipGetPlatform(void)
{
    const NvRmPrivChipInfo *chip = GetChipInfo();
    NV_ASSERT(chip);
    return chip->Platform;
}

