/*
 * Copyright (c) 2009-2013, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvos.h"
#include "aos.h"
#include "nvaboot.h"
#include "nvappmain.h"

extern NvError NvBDKTestServer(NvAbootHandle hAboot);

NvError NvAppMain(int argc, char *argv[])
{
    NvU32 EnterRecoveryMode = 1;
    NvU32 SecondaryBootDevice = 0;
    NvError e = NvSuccess;
    NvAbootHandle gs_hAboot = NULL;

    NvOsDebugPrintf("\nInitiating NvBDKTestServer\n");

    e = NvAbootOpen(&gs_hAboot, &EnterRecoveryMode, &SecondaryBootDevice);

    if (e != NvSuccess)
    {
        NvOsDebugPrintf("Failed to initialize Aboot\n");
        goto fail;
    }

    if (!NvOdmPlatformConfigure(NvOdmPlatformConfig_BlStart))
    {
         goto fail;
    }

    if (EnterRecoveryMode)
    {
        NvOsDebugPrintf("Entering NvFlash recovery mode / NvBDKTestServer\n");
        NV_CHECK_ERROR_CLEANUP(NvBDKTestServer(gs_hAboot));
    }

fail:
    if (e != NvSuccess)
        NvOsDebugPrintf("Unrecoverable bootloader error (0x%08x).\n", e);
    NV_ABOOT_BREAK();
}
