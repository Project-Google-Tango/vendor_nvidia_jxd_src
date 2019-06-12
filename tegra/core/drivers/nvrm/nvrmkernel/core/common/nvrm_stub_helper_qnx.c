/*
 * Copyright (c) 2011-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include <fcntl.h>
#include "nvos.h"

int g_nvmapfd = -1;

static void __attribute__ ((constructor)) nvrm_init(void)
{
    g_nvmapfd = open("/dev/nvmap", O_RDWR);

    if (g_nvmapfd < 0)
        NvOsDebugPrintf("\n\n\n****nvmap device open failed****\n\n\n");
}

static void __attribute__ ((destructor)) nvrm_fini(void)
{
    if (g_nvmapfd >= 0)
    {
        close(g_nvmapfd);
        g_nvmapfd = -1;
    }
}
