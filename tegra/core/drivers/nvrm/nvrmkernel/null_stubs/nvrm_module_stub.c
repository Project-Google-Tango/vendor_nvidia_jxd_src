/*
 * Copyright (c) 2009-2013 NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#define NV_IDL_IS_STUB

#include "nvcommon.h"
#include "nvos.h"
#include "nvassert.h"
#include "nvrm_chip_private.h"
#include "nvrm_module.h"
#include "nvrm_chipid.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define HW_HAS_NEW_GRAPHICS \
                ((chipId.Id != NVRM_AP20_ID) && \
                (chipId.Id != NVRM_T30_ID) && \
                (chipId.Id != NVRM_T114_ID) && \
                (chipId.Id != NVRM_T148_ID))

static NvError ReadIntFromFile( const char *filename, NvU32 *value )
{
    char buffer[257] = { '\0' };
    int fd, readcount, scancount;
    NvU32 rval = 0;

    fd = open(filename, O_RDONLY);
    if (fd == -1)
    {
        return NvError_BadValue;
    }
    readcount = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);
    if (readcount <= 0)
    {
        return NvError_BadValue;
    }

    scancount = sscanf(buffer, "%d", &rval);
    if (scancount != 1)
    {
        return NvError_BadValue;
    }
    *value = rval;
    return NvError_Success;
}

NvRmChipIdLimited NvRmPrivGetChipIdLimited( void )
{
    NvError rval1 = NvError_FileReadFailed, rval2 = NvError_FileReadFailed;
    static NvRmChipIdLimited gs_ChipId = { 0, 0 };
    const char *chip_FusePaths[] = {
#if NVOS_IS_QNX
        "/tmp/tegra_chip_id",
        "/tmp/tegra_chip_rev"
#else
        "/sys/module/tegra_fuse/parameters/tegra_chip_id",
        "/sys/module/tegra_fuse/parameters/tegra_chip_rev",
        "/sys/module/fuse/parameters/tegra_chip_id",
        "/sys/module/fuse/parameters/tegra_chip_rev",
        "/sys/devices/soc0/soc_id",
        "/sys/devices/soc0/revision"
#endif
    };
    NvU32 i;

    if (gs_ChipId.Id)
        return gs_ChipId;

    for (i = 0; rval1 != NvError_Success && rval2 != NvError_Success
        && i < NV_ARRAY_SIZE(chip_FusePaths) / 2; i++)
    {
        rval1 = ReadIntFromFile(chip_FusePaths[i * 2],  &(gs_ChipId.Id));
        rval2 = ReadIntFromFile(chip_FusePaths[i * 2 + 1], &(gs_ChipId.Revision));
    }

    if (rval1 != NvError_Success || rval2 != NvError_Success)
    {
        NvOsDebugPrintf("%s: Could not read Tegra chip id/rev \r\n", __func__);
        NvOsDebugPrintf("Expected on kernels without fuse support, using Tegra2\n");
        gs_ChipId.Id = NVRM_AP20_ID;
        gs_ChipId.Revision = 3;
    }

    return gs_ChipId;
}

NvError NvRmModuleGetCapabilities(
    NvRmDeviceHandle hDeviceHandle,
    NvRmModuleID Module,
    NvRmModuleCapability *pCaps,
    NvU32 NumCaps,
    void **Capability)
{
    NvU32 i;
    NvRmChipIdLimited chipId;
    NvRmModuleCapability *cap;
    void *ret = 0;
    NvRmModulePlatform curPlatform = NvRmModulePlatform_Silicon;

    NvU32 ver_tab[][2] = {
        [NvRmModuleID_Mpe] = { 1, 2 },
        [NvRmModuleID_BseA] = { 1, 1 },
        [NvRmModuleID_Display] = { 1, 3 },
        [NvRmModuleID_Spdif] = { 1, 0 },
        [NvRmModuleID_I2s] = { 1, 1 },
        [NvRmModuleID_Misc] = { 2, 0 },
        [NvRmModuleID_Vde] = { 1, 2 },
        [NvRmModuleID_Isp] = { 2, 0 },
        [NvRmModuleID_Vi] = { 1, 1 },
        [NvRmModuleID_3D] = { 1, 2 },
        [NvRmModuleID_2D] = { 1, 1 },
        [NvRmPrivModuleID_Gart] = { 1, 1 },
        [NvRmModuleID_Vp8] = { 1, 0 },
        [NvRmModuleID_MSENC] = { 2, 0 },
        [NvRmModuleID_Vic] = { 3, 0 },
        [NvRmModuleID_Gpu] = { 1, 0 },
    };

    chipId = NvRmPrivGetChipIdLimited();

    Module = NVRM_MODULE_ID_MODULE(Module);

#if PLATFORM_SIMULATION
    curPlatform = NvRmModulePlatform_Simulation;
#endif

#if PLATFORM_EMULATION
    curPlatform = NvRmModulePlatform_Emulation;
#endif

    if (Module >= NV_ARRAY_SIZE(ver_tab) || !ver_tab[Module][0])
    {
        NvOsDebugPrintf("%s: MOD[%u] not implemented!\n", __func__, Module);
        return NvError_NotSupported;
    }

    switch ( Module )
    {
        case NvRmModuleID_2D:
            if (HW_HAS_NEW_GRAPHICS)
            {
                ver_tab[Module][0] = 0;
            }
            break;
        case NvRmModuleID_3D:
            if (chipId.Id == NVRM_T30_ID)
                ver_tab[Module][1] = 3;
            if (chipId.Id == NVRM_T114_ID)
                ver_tab[Module][1] = 4;
            if (chipId.Id == NVRM_T148_ID)
                ver_tab[Module][1] = 5;
            if (HW_HAS_NEW_GRAPHICS)
            {
                ver_tab[Module][0] = 0;
            }
            break;
        case NvRmModuleID_Vde:
            if (chipId.Id == NVRM_T30_ID)
                ver_tab[Module][1] = 3;
            if (chipId.Id == NVRM_T114_ID)
            {
                ver_tab[Module][0] = 3;
                ver_tab[Module][1] = 0;
            }
            if (chipId.Id == NVRM_T148_ID)
            {
                ver_tab[Module][0] = 4;
                ver_tab[Module][1] = 0;
            }
            if (chipId.Id == NVRM_T124_ID)
            {
                ver_tab[Module][0] = 5;
                ver_tab[Module][1] = 0;
            }
            break;
        case NvRmModuleID_Display:
            if (chipId.Id >= NVRM_T30_ID)
                ver_tab[Module][1] = 4;
            break;
        case NvRmPrivModuleID_Gart:
            if (chipId.Id >= NVRM_T30_ID)
                /* T30+ don't have a GART (although they do have an SMMU) */
                ver_tab[Module][0] = 0;
            break;
        case NvRmModuleID_Vic:
        case NvRmModuleID_Gpu:
            if (!HW_HAS_NEW_GRAPHICS)
            {
                // before T124, there is neither GPU nor Vic
                ver_tab[Module][0] = 0;
            }
            break;
        case NvRmModuleID_Mpe:
            if ((chipId.Id != NVRM_AP20_ID) && (chipId.Id != NVRM_T30_ID))
                ver_tab[Module][0] = 0;
            break;
        case NvRmModuleID_MSENC:
            if ((chipId.Id == NVRM_AP20_ID) || (chipId.Id == NVRM_T30_ID))
                ver_tab[Module][0] = 0;
            if (chipId.Id == NVRM_T124_ID)
            {
                ver_tab[Module][0] = 3;
                ver_tab[Module][1] = 1;
            }
            if (chipId.Id == NVRM_T148_ID)
                ver_tab[Module][0] = 3;
            break;
        case NvRmModuleID_Vp8:
            if ( chipId.Id != NVRM_AP20_ID && chipId.Id != NVRM_T30_ID )
            {
                const char * szVP8Path[] = {
                    "/sys/module/tegra_fuse/parameters/tegra_fuse_vp8_enable",
                    "/sys/module/fuse/parameters/tegra_fuse_vp8_enable",
                    "/sys/devices/soc0/vp8_enable",
                    NULL
                };
                NvU32 vp8_val, i = 0;
                NvError rval1;
                do
                {
                    rval1 = ReadIntFromFile(szVP8Path[i++], &vp8_val);
                } while (rval1 != NvError_Success && szVP8Path[i]);
                if( rval1 != NvError_Success || vp8_val != 1 )
                    ver_tab[Module][0] = 0;
            }
            else
            {
                ver_tab[Module][0] = 0;
            }
            break;
        case NvRmModuleID_Vi:
            if ((chipId.Id == NVRM_T114_ID) ||
                (chipId.Id == NVRM_T148_ID))
                ver_tab[Module][1] = 2;
            else if (chipId.Id == NVRM_T124_ID)
                ver_tab[Module][0] = 2;
            break;
        case NvRmModuleID_Isp:
            if (chipId.Id == NVRM_AP20_ID)
                ver_tab[Module][0] = 1;
            else if ((chipId.Id == NVRM_T114_ID) ||
                     (chipId.Id == NVRM_T30_ID)  ||
                     (chipId.Id == NVRM_T148_ID))
                ver_tab[Module][0] = 2;
            else if (chipId.Id == NVRM_T124_ID)
                ver_tab[Module][0] = 3;
            break;
        default:
            /* nothing */
            break;
    }

    // Loop through the caps and return the last config that matches
    for (i=0; i<NumCaps; i++)
    {
        cap = &pCaps[i];

        if( cap->MajorVersion != ver_tab[Module][0] ||
            cap->MinorVersion != ver_tab[Module][1] ||
            cap->Platform != curPlatform )
        {
            continue;
        }

        if( (cap->EcoLevel == 0) || (cap->EcoLevel < chipId.Revision ))
        {
            ret = cap->Capability;
        }
    }

    if( !ret )
    {
        *Capability = 0;
        return NvError_NotSupported;
    }

    *Capability = ret;
    return NvSuccess;
}

void NvRmModuleReset(NvRmDeviceHandle hRmDeviceHandle, NvRmModuleID Module)
{
    NvOsDebugPrintf("deassert %s MOD[%u] INST[%u]\n", __func__,
                    NVRM_MODULE_ID_MODULE(Module),
                    NVRM_MODULE_ID_INSTANCE(Module));
}

NvU32 NvRmModuleGetNumInstances(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmModuleID Module)
{
    NvRmChipIdLimited chipId;

    chipId = NvRmPrivGetChipIdLimited();

    switch (NVRM_MODULE_ID_MODULE(Module))
    {
    case NvRmModuleID_I2s:
        return 4;
    case NvRmModuleID_Display:
        return 2;
    case NvRmModuleID_2D:
        if (HW_HAS_NEW_GRAPHICS)
            return 0;
        return 1;
    case NvRmModuleID_3D:
        if (chipId.Id == NVRM_T30_ID)
            return 2;
        if (HW_HAS_NEW_GRAPHICS)
            return 0;
        return 1;
    case NvRmModuleID_Vic:
    case NvRmModuleID_Gpu:
        if (HW_HAS_NEW_GRAPHICS)
            return 1;
        return 0;
    case NvRmModuleID_Avp:
    case NvRmModuleID_GraphicsHost:
    case NvRmModuleID_Vcp:
    case NvRmModuleID_Isp:
    case NvRmModuleID_Vi:
    case NvRmModuleID_Epp:
    case NvRmModuleID_Spdif:
    case NvRmModuleID_Vde:
    case NvRmModuleID_Hdcp:
    case NvRmModuleID_Hdmi:
    case NvRmModuleID_Tvo:
    case NvRmModuleID_Dsi:
    case NvRmModuleID_BseA:
        return 1;
    case NvRmModuleID_Mpe:
        if (chipId.Id != NVRM_AP20_ID && chipId.Id != NVRM_T30_ID)
            return 0;

        return 1;
    case NvRmModuleID_MSENC:
    case NvRmModuleID_TSEC:
        if (chipId.Id == NVRM_AP20_ID || chipId.Id == NVRM_T30_ID)
            return 0;

        return 1;
    case NvRmModuleID_XUSB_HOST:
        if (chipId.Id == NVRM_AP20_ID || chipId.Id == NVRM_T30_ID)
            return 0;

        return 1;
    default:
        NvOsDebugPrintf("%s: MOD[%u] not implemented\n", __func__,
                        NVRM_MODULE_ID_MODULE(Module));
        return 1;
    }
}

void NvRmModuleGetBaseAddress(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmModuleID Module,
    NvRmPhysAddr *pBaseAddress,
    NvU32 *pSize)
{
    NvRmPhysAddr base;
    NvU32 size;
    NvRmChipIdLimited chipId;
    chipId = NvRmPrivGetChipIdLimited();

    switch (NVRM_MODULE_ID_MODULE(Module))
    {
    case NvRmModuleID_GraphicsHost:
        base = 0x50000000;
        size = 144*1024;
        break;
    case NvRmModuleID_Display:
        base = 0x54200000 + (NVRM_MODULE_ID_INSTANCE(Module))*0x40000;
        size = 256*1024;
        break;
    case NvRmModuleID_Mpe:
        if (chipId.Id != NVRM_AP20_ID && chipId.Id != NVRM_T30_ID)
            goto defaultcase;

        base = 0x54040000;
        size = 256 * 1024;
        break;
    case NvRmModuleID_MSENC:
        if (chipId.Id == NVRM_AP20_ID || chipId.Id == NVRM_T30_ID)
            goto defaultcase;

        base = 0x544c0000;
        size = 256*1024;
        break;
    case NvRmModuleID_Dsi:
        base = 0x54300000;
        size = 256*1024;
        break;
    case NvRmModuleID_Vde:
        base = 0x6001a000;
        if (chipId.Id == NVRM_T124_ID)
            base = 0x60030000;
        size = 0x3c00;
        break;
    case NvRmModuleID_ExceptionVector:
        base = 0x6000f000;
        size = 4096;
        break;
    case NvRmModuleID_FlowCtrl:
        base = 0x60007000;
        size = 32;
        break;
    case NvRmPrivModuleID_ExternalMemoryController:
        if ((chipId.Id == NVRM_AP20_ID) || (chipId.Id == NVRM_T30_ID))
        {
              base = 0x7000f400;
              size = 1024;
        }
        else if ((chipId.Id == NVRM_T114_ID) || (chipId.Id == NVRM_T148_ID))
        {
              base = 0x7001b000;
              size = 2048;
        }
        else
        {
              base = 0x7001b000;
              size = 4096;
        }
        break;
    case NvRmPrivModuleID_ClockAndReset:
        base = 0x60006000;
        size = 4096;
        break;
    default:
defaultcase:
        NvOsDebugPrintf("%s: MOD[%u] INST[%u] not implemented\n",
                        __func__, NVRM_MODULE_ID_MODULE(Module),
                        NVRM_MODULE_ID_INSTANCE(Module));
        base = ~0;
        size = 0;
    }

    if (pBaseAddress)
        *pBaseAddress = base;
    if (pSize)
        *pSize = size;
}

NvError NvRmPrivGpuGetInfo(
    NvRmDeviceHandle hRm,
    NvRmPrivGpuInfo *pGpuInfo)
{
    NvError rval1, rval2, rval;
    NvRmPrivGpuInfo GpuInfo;

    if (!hRm || !pGpuInfo)
        return NvError_BadParameter;

    rval = NvSuccess;

#if NVOS_IS_QNX
    rval1 = ReadIntFromFile("/tmp/tegra_gpu_num_pixel_pipes",
                &GpuInfo.NumPixelPipes);
    rval2 = ReadIntFromFile("/tmp/tegra_gpu_num_alus_per_pixel_pipe",
                &GpuInfo.NumALUsPerPixelPipe);
#else
    rval1 = ReadIntFromFile("/sys/module/tegra_fuse/parameters/tegra_gpu_num_pixel_pipes",
                &GpuInfo.NumPixelPipes);
    rval2 = ReadIntFromFile("/sys/module/tegra_fuse/parameters/tegra_gpu_num_alus_per_pixel_pipe",
                &GpuInfo.NumALUsPerPixelPipe);
#endif
    if (rval1 != NvError_Success || rval2 != NvError_Success)
    {
        rval1 = ReadIntFromFile("/sys/module/fuse/parameters/tegra_gpu_num_pixel_pipes",
                    &GpuInfo.NumPixelPipes);
        rval2 = ReadIntFromFile("/sys/module/fuse/parameters/tegra_gpu_num_alus_per_pixel_pipe",
                    &GpuInfo.NumALUsPerPixelPipe);
        if (rval1 != NvError_Success || rval2 != NvError_Success)
        {
            NvOsDebugPrintf("%s: Could not read tegra_gpu_num_pixel_pipes or \
                tegra_gpu_num_alus_per_pixel_pipe \r\n", __func__);
            pGpuInfo->NumPixelPipes = 1;
            pGpuInfo->NumALUsPerPixelPipe = 1;
            rval = NvError_BadValue;
        }
    }
    else
    {
        pGpuInfo->NumPixelPipes = GpuInfo.NumPixelPipes;
        pGpuInfo->NumALUsPerPixelPipe = GpuInfo.NumALUsPerPixelPipe;
    }

    return rval;
}
