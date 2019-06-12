/*
 * Copyright (c) 2007 - 2011 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvassert.h"
#include "nvos.h"
#include "nvutil.h"
#include "nvrm_moduleloader.h"

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <linux/tegra_avp.h>
#include <sys/ioctl.h>
#include <string.h>

static int avp_fd = -1;

static NvError NvRmIoctl(int req, void *op)
{
    int err;
    if (avp_fd < 0) {
        avp_fd = open("/dev/tegra_avp", O_RDWR);
        if (avp_fd < 0) {
            err = errno;
            NvOsDebugPrintf("%s: open /dev/tegra_avp failed (%d): %s\n", __func__, err,
                            strerror(err));
            return NvError_KernelDriverNotFound;
        }
    }

    err = ioctl(avp_fd, req, op);
    if (err != 0)
        NvOsDebugPrintf("%s: req=%x err=%d\n", __func__, req, err);
    NV_ASSERT(err <= 0); /* This ioctl() never returns positive value */

    return (err == 0) ? NvSuccess : NvError_IoctlFailed;
}

NvError NvRmFreeLibrary( NvRmLibraryHandle hLibHandle )
{
    return NvRmIoctl(TEGRA_AVP_IOCTL_UNLOAD_LIB, hLibHandle);
}

NvError NvRmGetProcAddress(NvRmLibraryHandle hLibHandle,
                           const char * pSymbolName, void* * pSymAddress)
{
    abort();
    return NvError_KernelDriverNotFound;
}

NvError NvRmLoadLibraryEx( NvRmDeviceHandle hDevice, const char * pLibName,
                           void* pArgs, NvU32 sizeOfArgs,
                           NvBool IsApproachGreedy,
                           NvRmLibraryHandle * hLibHandle )
{
    struct tegra_avp_lib lib_desc = {{0}, 0, 0, 0, 0};
    NvError error;

    strncpy((char *)lib_desc.name, (char *)pLibName, TEGRA_AVP_LIB_MAX_NAME);
    lib_desc.args = pArgs;
    lib_desc.args_len = sizeOfArgs;
    lib_desc.greedy = IsApproachGreedy;

    error = NvRmIoctl(TEGRA_AVP_IOCTL_LOAD_LIB, (void *)&lib_desc);
    if (error == NvSuccess) {
        *hLibHandle = (NvRmLibraryHandle)lib_desc.handle;
    }
    return error;
}

NvError NvRmLoadLibrary( NvRmDeviceHandle hDevice, const char * pLibName,
                         void* pArgs, NvU32 sizeOfArgs,
                         NvRmLibraryHandle * hLibHandle )
{
    return NvRmLoadLibraryEx(hDevice, pLibName, pArgs, sizeOfArgs, NV_FALSE,
                             hLibHandle);
}

extern NvError NvRmPrivInitModuleLoaderRPC(NvRmDeviceHandle hDevice);
extern void NvRmPrivDeInitModuleLoaderRPC(void);

NvError NvRmPrivInitModuleLoaderRPC(NvRmDeviceHandle hDevice)
{
    return NvSuccess;
}

void NvRmPrivDeInitModuleLoaderRPC()
{
}
