/*
 * Copyright (c) 2011 - 2012 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include <fcntl.h>
#include <qnx/nvmap_devctls.h>
#include "nvcommon.h"
#include "nvrm_memmgr.h"
#include "nvrm_heap.h"

int g_nvmapfd = -1;

NvRmPrivHeap *NvRmPrivHeapGartInit(NvRmDeviceHandle hRmDevice)
{
    g_nvmapfd = open(NVMAP_DEV_NODE, O_RDWR);

    if (g_nvmapfd < 0)
        NvOsDebugPrintf("\n\n\n****nvmap device open failed****\n\n\n");
    /*
     * NVMAP-NOTE: Gart is not supported in nvmap. This function is called from
     * NvRmOpenNew() and we just open the nvmap device file for client stub.
     */
    return NULL;
}

void NvRmPrivHeapGartDeinit(void)
{
    if (g_nvmapfd >= 0)
    {
        close(g_nvmapfd);
        g_nvmapfd = -1;
    }
    /*
     * NVMAP-NOTE: See NvRmPrivHeapGartInit()
     */
}

void NvRmPrivGartSuspend(NvRmDeviceHandle hDevice)
{
}

void NvRmPrivGartResume(NvRmDeviceHandle hDevice)
{
}

NvU8 NvRmMemRd08(NvRmMemHandle hMem, NvU32 Offset)
{
    NvU8 buffer[4];
    NvRmMemRead(hMem, Offset, buffer, sizeof(buffer[0]));
    return buffer[0];
}

NvU16 NvRmMemRd16(NvRmMemHandle hMem, NvU32 Offset)
{
    NvU16 buffer[2];
    NV_ASSERT(!(Offset & 1));
    NvRmMemRead(hMem, Offset, buffer, sizeof(buffer[0]));
    return buffer[0];
}

NvU32 NvRmMemRd32(NvRmMemHandle hMem, NvU32 Offset)
{
    NvU32 buffer[1];
    NV_ASSERT(!(Offset & 3));
    NvRmMemRead(hMem, Offset, buffer, sizeof(buffer[0]));
    return buffer[0];
}

void NvRmMemWr08(NvRmMemHandle hMem, NvU32 Offset, NvU8 Data)
{
    NvU8 buffer[4];
    buffer[0] = Data;
    NvRmMemWrite(hMem, Offset, buffer, sizeof(buffer[0]));
}

void NvRmMemWr16(NvRmMemHandle hMem, NvU32 Offset, NvU16 Data)
{
    NvU16 buffer[2];
    buffer[0] = Data;
    NV_ASSERT(!(Offset & 1));
    NvRmMemWrite(hMem, Offset, buffer, sizeof(buffer[0]));
}

void NvRmMemWr32(NvRmMemHandle hMem, NvU32 Offset, NvU32 Data)
{
    NvU32 buffer[1];
    buffer[0] = Data;
    NV_ASSERT(!(Offset & 3));
    NvRmMemWrite(hMem, Offset, buffer, sizeof(buffer[0]));
}
