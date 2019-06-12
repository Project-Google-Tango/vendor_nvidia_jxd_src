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
#include <sys/ioctl.h>

#include "nvavp.h"
#include "linux/nvavp_ioctl.h"


#define NVAVP_DEVICE "/dev/tegra_avpchannel"

#define NVAVP_AUDIO_DEVICE "/dev/tegra_audio_avpchannel"

static int s_nvmap_fd = -1;

void __attribute__ ((constructor)) nvavp_init(void);
void __attribute__ ((constructor)) nvavp_init(void)
{
    s_nvmap_fd = NvRm_MemmgrGetIoctlFile();
}

void __attribute__ ((destructor)) nvavp_fini(void);
void __attribute__ ((destructor)) nvavp_fini(void)
{
}

NvError NvAvpAudioOpen(NvAvpHandle *phAvp)
{
    int fd;
    struct nvavp_set_nvmap_fd_args arg;

    if (!phAvp)
        return NvError_BadParameter;

    fd = open(NVAVP_AUDIO_DEVICE, O_RDWR);
    if (fd == -1)
        return NvError_ResourceError;

    arg.fd = s_nvmap_fd;

    if (ioctl(fd, (int)NVAVP_IOCTL_SET_NVMAP_FD, &arg))
    {
        printf("%s: NVAVP_IOCTL_SET_NVMAP_FD failed (%s)\n", __func__, strerror(errno));
        close(fd);
        return NvError_BadParameter;
    }

    *phAvp = (NvAvpHandle)fd;

    return NvSuccess;
}

NvError NvAvpAudioEnableClocks(
    NvAvpHandle hAvp,
    NvU32 ModuleId)
{
    int fd = (int) hAvp;
    struct nvavp_clock_args args;

    args.id = ModuleId;

    if (ioctl(fd, (int)NVAVP_IOCTL_ENABLE_AUDIO_CLOCKS, &args))
    {
        printf("%s: NVAVP_IOCTL_ENABLE_AUDIO_CLOCKS failed (%s)\n", __func__, strerror(errno));
        close(fd);
        return NvError_BadParameter;
    }
    return NvSuccess;
}

NvError NvAvpAudioDisableClocks(
    NvAvpHandle hAvp,
    NvU32 ModuleId)
{
    int fd = (int) hAvp;
    struct nvavp_clock_args args;

    args.id = ModuleId;

    if (ioctl(fd, (int)NVAVP_IOCTL_DISABLE_AUDIO_CLOCKS, &args))
    {
        printf("%s: NVAVP_IOCTL_DISABLE_AUDIO_CLOCKS failed (%s)\n", __func__, strerror(errno));
        close(fd);
        return NvError_BadParameter;
    }
    return NvSuccess;
}

NvError NvAvpOpen(NvAvpHandle *phAvp)
{
    int fd;
    struct nvavp_set_nvmap_fd_args arg;

    if(!phAvp)
        return NvError_BadParameter;

    fd = open(NVAVP_DEVICE, O_RDWR);
    if (fd == -1) {
        return NvError_ResourceError;
    }

    arg.fd = s_nvmap_fd;

    if (ioctl(fd, (int)NVAVP_IOCTL_SET_NVMAP_FD, &arg))
    {
        printf("%s: NVAVP_IOCTL_SET_NVMAP_FD failed (%s)\n", __func__, strerror(errno));
        close(fd);
        return NvError_BadParameter;
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

NvError NvAvpGetClockRate(
    NvAvpHandle hAvp,
    NvU32 ModuleId,
    NvU32 *pRate)
{
    int fd = (int) hAvp;
    struct nvavp_clock_args args;

    args.id = ModuleId;

    if (ioctl(fd, (int)NVAVP_IOCTL_GET_CLOCK, &args))
    {
        printf("%s: NVAVP_IOCTL_GET_CLOCK failed (%s)\n", __func__, strerror(errno));
        return NvError_BadParameter;
    }

    *pRate = args.rate;

    return NvSuccess;
}

NvError NvAvpSetClockRate(
    NvAvpHandle hAvp,
    NvU32 ModuleId,
    NvU32 Rate,
    NvU32 *pFreqSet)
{
    int fd = (int) hAvp;
    struct nvavp_clock_args args;

    args.id = ModuleId;
    args.rate = Rate;

    if (ioctl(fd, (int)NVAVP_IOCTL_SET_CLOCK, &args))
    {
        printf("%s: NVAVP_IOCTL_SET_CLOCK failed (%s)\n", __func__, strerror(errno));
        return NvError_BadParameter;
    }

    if (pFreqSet)
        *pFreqSet = args.rate;

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
    int fd = (int) hAvp;
    struct nvavp_pushbuffer_submit_hdr args;
    struct nvavp_syncpt syncpt;

    if (!pCommandBuf || (NumRelocations && !pRelocations))
        return NvError_BadValue;


    args.cmdbuf.mem = (__u32)pCommandBuf->hMem;
    args.cmdbuf.offset = (__u32)pCommandBuf->Offset;
    args.cmdbuf.words = (__u32)pCommandBuf->Words;
    args.relocs = (struct nvavp_reloc *)pRelocations;
    args.num_relocs = NumRelocations;
    args.flags = NVAVP_FLAG_NONE;
    args.syncpt = NULL;

    if(pPbFence)
        args.syncpt = &syncpt;

    if (ioctl(fd, (int)NVAVP_IOCTL_PUSH_BUFFER_SUBMIT, &args))
    {
        printf("%s: NVAVP_IOCTL_PUSH_BUFFER_SUBMIT failed (%s)\n", __func__, strerror(errno));
        return NvError_BadParameter;
    }

    if(pPbFence)
    {
        pPbFence->SyncPointID = syncpt.id;
        pPbFence->Value = syncpt.value;
    }

    return NvSuccess;
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
    struct nvavp_pushbuffer_submit_hdr args;
    struct nvavp_syncpt syncpt = {NVRM_INVALID_SYNCPOINT_ID, 0};

    if (!pCommandBuf || (NumRelocations && !pRelocations))
        return NvError_BadValue;


    args.cmdbuf.mem = (__u32)pCommandBuf->hMem;
    args.cmdbuf.offset = (__u32)pCommandBuf->Offset;
    args.cmdbuf.words = (__u32)pCommandBuf->Words;
    args.relocs = (struct nvavp_reloc *)pRelocations;
    args.num_relocs = NumRelocations;
    args.flags = NVAVP_FLAG_NONE;
    args.syncpt = NULL;

    if (flags & NVAVP_CLIENT_UCODE_EXT)
    {
        args.flags |= NVAVP_UCODE_EXT;
    }

    if(pPbFence)
        args.syncpt = &syncpt;

    if (ioctl(fd, (int)NVAVP_IOCTL_PUSH_BUFFER_SUBMIT, &args))
    {
        printf("%s: NVAVP_IOCTL_PUSH_BUFFER_SUBMIT failed (%s)\n", __func__, strerror(errno));
        return NvError_BadParameter;
    }

    if(pPbFence)
    {
        pPbFence->SyncPointID = syncpt.id;
        pPbFence->Value = syncpt.value;
    }

    return NvSuccess;
}

NvError NvAvpGetSyncPointID(
    NvAvpHandle hAvp,
    NvU32 *pSyncPointID)
{
    int fd = (int) hAvp;

    if (!pSyncPointID)
        return NvError_BadValue;

    if (ioctl(fd, (int)NVAVP_IOCTL_GET_SYNCPOINT_ID, pSyncPointID))
    {
        printf("%s: NVAVP_IOCTL_GET_SYNCPOINT_ID failed (%s)\n", __func__, strerror(errno));
        return NvError_BadParameter;
    }
    return NvSuccess;
}

NvError NvAvpWakeAvp(
    NvAvpHandle hAvp)
{
    int fd = (int) hAvp;

    if (ioctl(fd, (int)NVAVP_IOCTL_WAKE_AVP, NULL))
    {
        printf("%s: NVAVP_IOCTL_WAKE_AVP failed (%s)\n", __func__, strerror(errno));
        return NvError_BadParameter;
    }

    return NvSuccess;
}

NvError NvAvpForceClockStayOn(
    NvAvpHandle hAvp,
    NvU32 flag)
{
    int fd = (int) hAvp;
    struct nvavp_clock_stay_on_state_args args;

    args.state = flag;

    if (ioctl(fd, (int)NVAVP_IOCTL_FORCE_CLOCK_STAY_ON, &args))
    {
        printf("%s: NVAVP_IOCTL_FORCE_CLOCK_STAY_ON failed (%s)\n", __func__, strerror(errno));
        return NvError_BadParameter;
    }
    return NvSuccess;
}

NvError NvAvpSetMinOnlineCpus(
    NvAvpHandle hAvp,
    NvU32 num)
{
    int fd = (int) hAvp;
    struct nvavp_num_cpus_args args;

    args.min_online_cpus = num;

    if (ioctl(fd, (int)NVAVP_IOCTL_SET_MIN_ONLINE_CPUS, &args))
    {
        printf("%s: NVAVP_IOCTL_SET_MIN_ONLINE_CPUS failed (%s)\n", __func__, strerror(errno));
        return NvError_BadParameter;
    }

    return NvSuccess;
}

NvError NvAvpMapIova(
    NvAvpHandle hAvp,
    NvRmMemHandle MemHandle,
    NvU32 *MapAddr)
{
    int fd = (int) hAvp;
    struct nvavp_map_args args;

    args.mem = (__s32)MemHandle;

    if (ioctl(fd, NVAVP_IOCTL_MAP_IOVA, &args))
    {
        printf("%s: failed with error %d (%s)\n", __func__, errno, strerror(errno));
        return NvError_BadParameter;
    }
    if (MapAddr)
        *MapAddr = args.addr;

    return NvSuccess;
}

NvError NvAvpUnMapIova(
    NvAvpHandle hAvp,
    NvRmMemHandle MemHandle,
    NvU32 MapAddr)
{
    int fd = (int) hAvp;
    struct nvavp_map_args args;

    args.mem = (__s32)MemHandle;
    args.addr = MapAddr;

    if (ioctl(fd, NVAVP_IOCTL_UNMAP_IOVA, &args))
    {
        printf("%s: failed with error (%s)\n", __func__, strerror(errno));
        return NvError_BadParameter;
    }

    return NvSuccess;
}
