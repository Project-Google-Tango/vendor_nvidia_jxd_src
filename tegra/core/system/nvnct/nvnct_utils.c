/*
 * Copyright (c) 2013, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvos.h"
#include "nverror.h"
#include "fastboot.h"
#include "nvaboot.h"
#include "nvodm_query.h"
#include "nvodm_services.h"
#include "nvboot_bit.h"
#include "nvddk_blockdevmgr.h"
#include "nvassert.h"
#include "nvpartmgr.h"
#include "nvstormgr.h"
#include "crc32.h"
#include "nct.h"
#include "arfuse.h"
#include "nvrm_hardware_access.h"

#define FUSE_PA_BASE 0x7000F800

#define NVBACK_READ_SPARE(bit)    \
    NV_READ32((((NvUPtr)(FUSE_PA_BASE)) + FUSE_SPARE_BIT_0_0 + (bit << 2)))

static NvU32 NvNctTID;

NvU32 NvNctGetTID(void)
{
    int bit;

    bit = NVBACK_READ_SPARE(0);

    if (!NvNctTID)
    {
        /* low 16bits */
        for (bit=12;bit<=27;bit++)
        {
            if (NVBACK_READ_SPARE(bit))
                NvNctTID |= 0x1;
            NvNctTID <<= 1;
        }
        /* mid 8bits */
        for (bit=35;bit<=42;bit++)
        {
            if (NVBACK_READ_SPARE(bit))
                NvNctTID |= 0x1;
            NvNctTID <<= 1;
        }
        /* high 8bits */
        for (bit=51;bit<=58;bit++)
        {
            if (NVBACK_READ_SPARE(bit))
                NvNctTID |= 0x1;
            NvNctTID <<= 1;
        }

        NvNctTID &= 0xFFFF;
        NvOsDebugPrintf("%s: TID: 0x%04x\n", __func__, NvNctTID);
    }

    return NvNctTID;
}

/* buffer length should be 4bytes aligned */
NvError NvNctEncryptData(NvU32 *src, NvU32 *dst, NvU32 length)
{
    NvError err = NvSuccess;
    NvU32 tid, cnt;

    if (length & 0x3)
        err = NvError_BadParameter;

    length = (length + 3) >> 2;
    tid = NvNctGetTID();

    while (length--)
    {
        *dst++ = *src++ ^ tid;
    }

    return err;
}

/* 4bytes dedicated */
NvU32 NvNctEncryptU32(NvU32 buf)
{
    NvU32 src = buf, dst;

    NvNctEncryptData(&src, &dst, sizeof(NvU32));
    return dst;
}

NvError NvNctVerifyCheckSum(nct_entry_type *entry)
{
    NvU32 crc;

    crc = NvComputeCrc32(0, (NvU8 *)entry,
                        sizeof(nct_entry_type)-sizeof(entry->checksum));
    if (crc != entry->checksum)
        return NvError_InvalidState;
    else
        return NvSuccess;
}

