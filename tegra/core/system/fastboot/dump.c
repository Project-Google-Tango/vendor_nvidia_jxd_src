/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvaboot.h"
#include "fastboot.h"
#include "nvodm_query.h"

#define NVDUMPER_DIRTY 0xdeadbeefU
#define NVDUMPER_CLEAN 0xf000caf3U
#define NVDUMPER_UNKNOWN 0U


/*
 * Get some information about the last reboot
 *
 * Returns 0 if the last reboot was clean (planned).
 * Returns 1 if the last reboot was dirty (unplanned).
 * Returns -1 if there is no information about the last reboot.
 */
NvBool LastRebootInfo(NvU32 NvDumperReserved)
{
    volatile NvU32 *ptr;
    NvU32 val;

    if (!NvDumperReserved)
    {
        NvOsDebugPrintf("nvdumper: Not supported!\n");
        return NV_FALSE;
    }

    ptr = (volatile NvU32*)NvDumperReserved;
    val = *ptr;
    if (val == NVDUMPER_DIRTY)
    {
        NvOsDebugPrintf("nvdumper: last reboot was dirty.\n");
        return NV_TRUE;
    }
    else if (val == NVDUMPER_CLEAN)
    {
        NvOsDebugPrintf("nvdumper: last reboot was clean.\n");
        return NV_FALSE;
    }
    NvOsDebugPrintf("nvdumper: No information found about last reboot... ");
    return NV_FALSE;
}

void ClearRebootInfo(NvU32 NvDumperReserved)
{
    volatile NvU32 *ptr;

    ptr = (volatile NvU32*)NvDumperReserved;
    *ptr = NVDUMPER_UNKNOWN;
}

#if ENABLE_NVDUMPER
/*
 * Determine the physical address of the nvdumper control block
 */
NvU32 CalcNvDumperReserved(NvAbootHandle hAboot)
{
    NvU64 Base, Size;

    NvAbootMemLayoutBaseSize(hAboot, NvAbootMemLayout_RamDump, &Base, &Size);
    NvOsDebugPrintf("nvdumper: base(0x%x) size(0x%x)\n", (NvU32)Base, (NvU32)Size);

    if (Size)
        return (NvU32)Base;
    else
    {
        NvOsDebugPrintf("nvdumper: Not Supported!\n");
        return 0;
    }
}
#endif

