/*
 * Copyright (c) 2011-2013 NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA CORPORATION
 * is strictly prohibited.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include "nvavp.h"
#include "qnx/nvavp_devctls.h"


#define NVAVP_DEVICE "/dev/tegra_avpchannel"

#define NVAVP_AUDIO_DEVICE "/dev/tegra_audio_avpchannel"


void __attribute__ ((constructor)) nvavp_init(void);
void __attribute__ ((constructor)) nvavp_init(void)
{
}

void __attribute__ ((destructor)) nvavp_fini(void);
void __attribute__ ((destructor)) nvavp_fini(void)
{
}

NvError NvAvpAudioOpen(NvAvpHandle *phAvp)
{
    return NvSuccess;
}

NvError NvAvpAudioEnableClocks(
    NvAvpHandle hAvp,
    NvU32 ModuleId)
{
    return NvSuccess;
}

NvError NvAvpAudioDisableClocks(
    NvAvpHandle hAvp,
    NvU32 ModuleId)
{
    return NvSuccess;
}

NvError NvAvpOpen(NvAvpHandle *phAvp)
{
    int fd;

    if(!phAvp)
        return NvError_BadParameter;

    fd = open(NVAVP_DEVICE, O_RDWR);
    if (fd == -1) {
        printf("%s: Error opening nvavp device (%s)\n", __func__, strerror(errno));
        return NvError_ResourceError;
    }

    *phAvp = (NvAvpHandle)fd;

    return NvSuccess;
}

NvError NvAvpClose(NvAvpHandle hAvp)
{
    int fd = (int) hAvp;

    close(fd);
    return NvSuccess;
}

NvError NvAvpSubmitBuffer(
    NvAvpHandle hAvp,
    const NvAvpCommandBuffer *pCommandBuf,
    const NvAvpRelocationEntry *pRelocations,
    NvU32 NumRelocations,
    NvRmFence *pPbFence
)
{
    return NvError_BadParameter;
}

NvError NvAvpSubmitBufferNew(
    NvAvpHandle hAvp,
    const NvAvpCommandBuffer *pCommandBuf,
    const NvAvpRelocationEntry *pRelocations,
    NvU32 NumRelocations,
    NvRmFence *pPbFence,
    NvU32 flags
)
{
    int fd = (int) hAvp;
    struct nvavp_submit_t args;

    if (!pCommandBuf || (NumRelocations && !pRelocations))
        return NvError_BadParameter;

    if (NumRelocations > NVAVP_MAX_RECLOCS) {
        printf("%s: No. of relocations too high!\n", __func__);
        return NvError_BadValue;
    }

    memcpy(&args.cmdbuf, pCommandBuf, sizeof(NvAvpCommandBuffer));

    memcpy(args.relocs, pRelocations, sizeof(NvAvpRelocationEntry) * NumRelocations);

    args.num_relocs = NumRelocations;

    if (pPbFence)
        args.fence_flag = NV_TRUE;

    args.fence.SyncPointID = NVRM_INVALID_SYNCPOINT_ID;
    args.fence.Value = 0;
    args.flags = NVAVP_FLAG_NONE;

    if (flags & NVAVP_CLIENT_UCODE_EXT)
    {
        args.flags |= NVAVP_UCODE_EXT;
    }


    if (devctl(fd, (int)NVAVP_DEVCTL_SUBMIT, &args, sizeof(struct nvavp_submit_t), NULL))
    {
        printf("%s: NVAVP_DEVCTL_SUBMIT failed (%s)\n", __func__, strerror(errno));
        return NvError_BadParameter;
    }

    if(pPbFence)
    {
        pPbFence->SyncPointID = args.fence.SyncPointID;
        pPbFence->Value = args.fence.Value;
    }

    return NvSuccess;
}

NvError NvAvpGetSyncPointID(
    NvAvpHandle hAvp,
    NvU32 *pSyncPointID)
{
    int fd = (int) hAvp;
    unsigned int args;

    if (!pSyncPointID)
        return NvError_BadValue;

    *pSyncPointID = NVRM_INVALID_SYNCPOINT_ID;

    if (devctl(fd, (int)NVAVP_DEVCTL_GET_SYNCPOINT_ID, &args, sizeof(unsigned int), NULL))
    {
        printf("%s: NVAVP_DEVCTL_GET_SYNCPOINT_ID failed (%s)\n", __func__, strerror(errno));
        return NvError_BadParameter;
    }

    *pSyncPointID = (NvU32)args;
    return NvSuccess;
}

NvError NvAvpGetClockRate(
    NvAvpHandle hAvp,
    NvU32 ModuleId,
    NvU32 *pRate)
{
    return NvSuccess;
}

NvError NvAvpSetClockRate(
    NvAvpHandle hAvp,
    NvU32 ModuleId,
    NvU32 Rate,
    NvU32 *pFreqSet)
{
    return NvSuccess;
}

NvError NvAvpWakeAvp(
    NvAvpHandle hAvp)
{
    return NvSuccess;
}

NvError NvAvpForceClockStayOn(
    NvAvpHandle hAvp,
    NvU32 flag)
{
    return NvSuccess;
}

NvError NvAvpSetMinOnlineCpus(
    NvAvpHandle hAvp,
    NvU32 num)
{
    return NvSuccess;
}

NvError NvAvpMapIova(
    NvAvpHandle hAvp,
    NvRmMemHandle MemHandle,
    NvU32 *MapAddr)
{
    return NvSuccess;
}

NvError NvAvpUnMapIova(
    NvAvpHandle hAvp,
    NvRmMemHandle MemHandle,
    NvU32 MapAddr)
{
    return NvSuccess;
}
