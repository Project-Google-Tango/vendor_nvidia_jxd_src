/*
 * Copyright (c) 2012 - 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvos.h"
#include "nvrm_module.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#if defined(CONFIG_TRUSTED_FOUNDATIONS)
#include "tee_client_api.h"
#endif

#define AP20_ID 0x20
#define T30_ID  0x30

typedef struct ChipIdRec
{
    NvU32 Id;
    NvU32 Revision;
} ChipId;

/*
 * Note: there's possible duplication of routines that might exist
 * elsewhere, but having them here, allows this static lib to not rely
 * on modules that customers may receive in source (i.e. modifiable).
 */

static NvError NvRmPrivSecureReadIntFromFile( const char *filename, NvU32 *value )
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

static ChipId NvRmPrivSecureGetChipId( void )
{
    static ChipId s_ChipId;
    ChipId chipId = { 0, 0 };
    NvError rval1 = NvError_FileReadFailed, rval2 = NvError_FileReadFailed;
    const char *chip_FusePaths[] = {
        "/sys/module/tegra_fuse/parameters/tegra_chip_id",
        "/sys/module/tegra_fuse/parameters/tegra_chip_rev",
        "/sys/module/fuse/parameters/tegra_chip_id",
        "/sys/module/fuse/parameters/tegra_chip_rev",
        "/sys/devices/soc0/soc_id",
        "/sys/devices/soc0/revision"
    };
    NvU32 i;

    if (s_ChipId.Id)
        return s_ChipId;

    for (i = 0; rval1 != NvError_Success && rval2 != NvError_Success
        && i < NV_ARRAY_SIZE(chip_FusePaths) / 2; i++)
    {
        rval1 = NvRmPrivSecureReadIntFromFile(chip_FusePaths[i * 2],  &chipId.Id);
        rval2 = NvRmPrivSecureReadIntFromFile(chip_FusePaths[i * 2 + 1], &chipId.Revision);
    }

    if (rval1 == NvError_Success && rval2 == NvError_Success)
    {
        s_ChipId = chipId;
    }
    else
    {
        NvOsDebugPrintf("%s: Could not read Tegra chip id/rev \r\n", __func__);
        NvOsDebugPrintf("Expected on kernels without fuse support, using Tegra2\n");
        s_ChipId.Id = AP20_ID;
        s_ChipId.Revision = 3;
    }

    return s_ChipId;
}

NvError NvRmCheckValidSecureState( NvRmDeviceHandle hDevice )
{
    static NvBool s_IsValidSet = NV_FALSE;
    static NvError s_Valid = NvError_Success;
    NvU32 OdmProdMode;

    if (s_IsValidSet)
        return s_Valid;

    s_IsValidSet = NV_TRUE;

    /* Determine if it's a ODM Production Mode part */
    if (NvRmPrivSecureReadIntFromFile("/sys/devices/platform/tegra-fuse/odm_production_mode", &OdmProdMode) != NvSuccess)
    {
        NvOsDebugPrintf("%s: Could not read ODM Production mode sysfs\n", __func__);
        s_Valid = NvError_AccessDenied;
        return s_Valid;
    }

    if (OdmProdMode)
    {
        NvBool isSecureOSBooted = NV_FALSE;
        ChipId chipId;

        /* Determine required SecureOS level based on Chip ID */
        chipId = NvRmPrivSecureGetChipId();
        if (chipId.Id >= T30_ID)
        {
#if defined(CONFIG_TRUSTED_FOUNDATIONS)
            TEEC_Context sContext;

            /* Is SecureOS loaded/running */
            if (TEEC_InitializeContext(NULL, &sContext) == TEEC_SUCCESS)
            {
                TEEC_FinalizeContext(&sContext);
                isSecureOSBooted = NV_TRUE;
            }
#endif
            /* Check for mismatch in expected SecureOS Level */
            if (isSecureOSBooted && (chipId.Revision <= 2))
            {
                NvOsDebugPrintf("%s: Tegra3 rev A02 (or older) should not run SecureOS\n", __func__);
                s_Valid = NvError_NotSupported;
            }
            else if (!isSecureOSBooted && (chipId.Revision >= 3))
            {
                NvOsDebugPrintf("%s: Tegra3 rev A03 (or newer) must run with SecureOS\n", __func__);
                s_Valid = NvError_NotSupported;
            }
        }
    }
    return s_Valid;
}
