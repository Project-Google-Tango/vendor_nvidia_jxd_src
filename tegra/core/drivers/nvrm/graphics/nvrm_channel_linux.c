/*
 * Copyright (c) 2010-2014 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvrm_channel.h"
#include "nvrm_memmgr.h"
#include "nvos.h"
#include "nvassert.h"
#include "nvrm_sched.h"
#include "linux/nvhost_ioctl.h"
#include "nvrm_channel_priv.h"

#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#if defined(ANDROID)
#include <sync/sync.h>
#endif

#define HOSTNODE_PREFIX "/dev/nvhost-"

/* Channel type */
enum NvRmChannelSyncptType
{
    /* Not known yet */
    NvRmChannelSyncptType_Unknown,
    /* Channel sync point id can change. */
    NvRmChannelSyncptType_Dynamic,
    /* Channel sync point id cannot change */
    NvRmChannelSyncptType_Static
};

struct NvRmChannelRec
{
    int fd;
    NvU32 in_submit;
    const char* device;
    NvU32 submit_version;
    /* Channel type - static or dynamic */
    int syncpt_type;
};

static int s_CtrlDev = -1;
static int s_CtrlVersion = 1;

void __attribute__ ((constructor)) nvrm_channel_init(void);
void __attribute__ ((constructor)) nvrm_channel_init(void)
{
    int err;
    struct nvhost_get_param_args args;
    int flags = O_RDWR | O_SYNC;

    /* Use O_CLOEXEC if it exists. This prevents fd leak when forking with
     * a channel open. */
#ifdef O_CLOEXEC
    flags |= O_CLOEXEC;
#endif

    s_CtrlDev = open(HOSTNODE_PREFIX "ctrl", flags);
    if (s_CtrlDev < 0)
    {
        NvOsDebugPrintf("Error: Can't open " HOSTNODE_PREFIX "ctrl\n");
    }

    /* We assume version 1 if we can't get it from kernel */
    err = ioctl(s_CtrlDev, NVHOST_IOCTL_CTRL_GET_VERSION, &args);
    if (!err)
        s_CtrlVersion = args.value;
}

void __attribute__ ((destructor)) nvrm_channel_fini(void);
void __attribute__ ((destructor)) nvrm_channel_fini(void)
{
    close(s_CtrlDev);
}

static int IndexOfNthSetBit(NvU32 Bits, int n)
{
    NvU32 i;
    for (i = 0; Bits; ++i, Bits >>= 1)
        if ((Bits & 1) && (n-- == 0))
                return i;
    return -1;
}

static struct {
    NvU16 module;
    NvU16 instance;
    const char *dev_node;
} s_DeviceNodeMap[] = {
    { NvRmModuleID_Display , 0, HOSTNODE_PREFIX "display" },
    { NvRmModuleID_2D,  0, HOSTNODE_PREFIX "gr2d" },
    { NvRmModuleID_3D,  0, HOSTNODE_PREFIX "gr3d" },
    { NvRmModuleID_Isp, 0, HOSTNODE_PREFIX "isp" },
    { NvRmModuleID_Vi,  0, HOSTNODE_PREFIX "vi" },
    { NvRmModuleID_Mpe, 0, HOSTNODE_PREFIX "mpe" },
    { NvRmModuleID_Dsi, 0, HOSTNODE_PREFIX "dsi" },
    { NvRmModuleID_MSENC, 0, HOSTNODE_PREFIX "msenc" },
    { NvRmModuleID_TSEC,  0, HOSTNODE_PREFIX "tsec" },
    { NvRmModuleID_Gpu, 0, HOSTNODE_PREFIX "gpu" },
    { NvRmModuleID_Vic, 0, HOSTNODE_PREFIX "vic" },
    { NvRmModuleID_Vi,  1, HOSTNODE_PREFIX "vi.1" },
    { NvRmModuleID_Isp, 1, HOSTNODE_PREFIX "isp.1" },
    { NvRmModuleID_Invalid, 0, NULL }
};

static const char* GetDeviceNode(NvRmModuleID mod)
{
    unsigned int i;
    NvU16 instance = NVRM_MODULE_ID_INSTANCE(mod);
    NvU16 module = NVRM_MODULE_ID_MODULE(mod);

    for (i=0; i < sizeof(s_DeviceNodeMap)/sizeof(s_DeviceNodeMap[0]); i++) {
        if ((s_DeviceNodeMap[i].module == module) &&
            (s_DeviceNodeMap[i].instance == instance)) {
            return s_DeviceNodeMap[i].dev_node;
        }
    }
    return NULL;
}

NvError
NvRmChannelOpen(
    NvRmDeviceHandle hDevice,
    NvRmChannelHandle *phChannel,
    NvU32 NumModules,
    const NvRmModuleID* pModuleIDs)
{
    NvRmChannelHandle ch;
    const char* node;
    int nvmap;
    int flags = O_RDWR;

    if (!NumModules)
        return NvError_BadParameter;

    node = GetDeviceNode(pModuleIDs[0]);
    if (!node)
    {
        NvOsDebugPrintf("Opening channel failed, unsupported module %d\n", pModuleIDs[0]);
        return NvError_BadParameter;
    }

    ch = NvOsAlloc(sizeof(*ch));
    if (!ch)
        return NvError_InsufficientMemory;
    NvOsMemset(ch, 0, sizeof(*ch));

    /* Use O_CLOEXEC if it exists. This prevents fd leak when forking with
     * a channel open. */
#ifdef O_CLOEXEC
    flags |= O_CLOEXEC;
#endif
    ch->fd = open(node, flags);
    if (ch->fd < 0)
    {
        NvOsFree(ch);
        NvOsDebugPrintf("Opening channel %s (0x%x) failed\n",
                        node, pModuleIDs[0]);
        return NvError_KernelDriverNotFound;
    }
    ch->device = node;
    ch->submit_version = s_CtrlVersion;

    /* Set channel syncpt type as unknown */
    ch->syncpt_type = NvRmChannelSyncptType_Unknown;

    nvmap = NvRm_MemmgrGetIoctlFile();
    if (ioctl(ch->fd, NVHOST_IOCTL_CHANNEL_SET_NVMAP_FD, &nvmap) < 0)
    {
        NvRmChannelClose(ch);
        NvOsDebugPrintf("Opening channel failed, can't set nvmap fd %d\n", pModuleIDs[0]);
        return NvError_KernelDriverNotFound;
    }

    *phChannel = ch;
    return NvSuccess;
}

void
NvRmChannelClose(
    NvRmChannelHandle ch)
{
    if (ch != NULL)
    {
        close(ch->fd);
        NvOsFree(ch);
    }
}

NvError
NvRmChannelSetSubmitTimeoutEx(
    NvRmChannelHandle ch,
    NvU32 SubmitTimeout,
    NvBool DisableDebugDump)
{
    struct nvhost_set_timeout_ex_args args_ext;
    struct nvhost_set_timeout_args args;
    NvError err = NvSuccess;

    args.timeout = SubmitTimeout;    /* in milliseconds */
    args_ext.timeout = SubmitTimeout;
    args_ext.flags = DisableDebugDump ?
        (1 << NVHOST_TIMEOUT_FLAG_DISABLE_DUMP) : 0;

    if (ch == NULL)
        err = NvError_NotInitialized;
    else if (ioctl(ch->fd, NVHOST_IOCTL_CHANNEL_SET_TIMEOUT_EX,
        &args_ext) < 0) {

        if (ioctl(ch->fd, NVHOST_IOCTL_CHANNEL_SET_TIMEOUT, &args) < 0) {
            NvOsDebugPrintf("Failed to set SubmitTimeout (%d ms)\n",
                SubmitTimeout);
            err = NvError_NotImplemented;
        }
    }

    return err;
}

NvError
NvRmChannelSetSubmitTimeout(
    NvRmChannelHandle ch,
    NvU32 SubmitTimeout)
{
    return NvRmChannelSetSubmitTimeoutEx(ch, SubmitTimeout, NV_FALSE);
}

#if NV_DEBUG
static NvBool
CheckSyncPointID(NvRmChannelHandle chan, NvU32 id)
{
    struct nvhost_get_param_args args;
    /*
     * We don't have a way to retrieve all syncpt ids for ids > 32. As this is
     * just a debug help, letting that pass.
     * */
    if (id > 31)
        return NV_TRUE;

    if (ioctl(chan->fd, NVHOST_IOCTL_CHANNEL_GET_SYNCPOINTS, &args) < 0)
    {
        NvOsDebugPrintf("CheckSyncPointID: "
                   "NvError_IoctlFailed with error code %d\n", errno);
        return NV_FALSE;
    }
    return ((1 << id) & args.value) ? NV_TRUE : NV_FALSE;
}
#endif

NvError
NvRmChannelSubmitOld(
    NvRmChannelHandle hChannel,
    const NvRmCommandBuffer *pCommandBufs, NvU32 NumCommandBufs,
    const NvRmChannelSubmitReloc *pRelocations, const NvU32 *pRelocShifts,
    NvU32 NumRelocations,
    const NvRmChannelWaitChecks *pWaitChecks, NvU32 NumWaitChecks,
    NvRmContextHandle hContext,
    const NvU32 *pContextExtra, NvU32 ContextExtraCount,
    NvBool NullKickoff, NvRmModuleID ModuleID,
    NvU32 StreamSyncPointIdx, NvU32 SyncPointWaitMask,
    NvU32 *pCtxChanged,
    NvU32 *pSyncPointValue,
    NvRmSyncPointDescriptor *SyncPointIncrs,
    NvU32 NumSyncPoints);
NvError
NvRmChannelSubmitOld(
    NvRmChannelHandle hChannel,
    const NvRmCommandBuffer *pCommandBufs, NvU32 NumCommandBufs,
    const NvRmChannelSubmitReloc *pRelocations, const NvU32 *pRelocShifts,
    NvU32 NumRelocations,
    const NvRmChannelWaitChecks *pWaitChecks, NvU32 NumWaitChecks,
    NvRmContextHandle hContext,
    const NvU32 *pContextExtra, NvU32 ContextExtraCount,
    NvBool NullKickoff, NvRmModuleID ModuleID,
    NvU32 StreamSyncPointIdx, NvU32 SyncPointWaitMask,
    NvU32 *pCtxChanged,
    NvU32 *pSyncPointValue,
    NvRmSyncPointDescriptor *SyncPointIncrs,
    NvU32 NumSyncPoints)
{
    struct nvhost32_submit_args args;
    struct nvhost_syncpt_incr incr[NVRM_MAX_SYNCPOINTS_PER_SUBMIT];
    NvU32 waitbases[NVRM_MAX_SYNCPOINTS_PER_SUBMIT];
    int ioctlret;
    int ioc = NVHOST32_IOCTL_CHANNEL_SUBMIT;
    NvU32 i;

    (void)hContext;
    (void)ModuleID;

    NV_ASSERT(!pContextExtra);
    NV_ASSERT(ContextExtraCount == 0);
    NV_ASSERT(!NullKickoff);
    NV_ASSERT(CheckSyncPointID(hChannel,
        SyncPointIncrs[StreamSyncPointIdx].SyncPointID));

    NvOsMemset(&args, 0, sizeof(args));
    args.num_syncpt_incrs = NumSyncPoints;
    args.syncpt_incrs = (uintptr_t)incr;
    for (i = 0; i < NumSyncPoints; ++i) {
        incr[i].syncpt_id = SyncPointIncrs[i].SyncPointID;
        incr[i].syncpt_incrs = SyncPointIncrs[i].Value;
        waitbases[i] = SyncPointIncrs[i].WaitBaseID;
    }
    if (pSyncPointValue)
        NvOsMemset(pSyncPointValue, 0,
            sizeof(*pSyncPointValue) * NumSyncPoints);
    args.num_cmdbufs = NumCommandBufs;
    args.num_relocs = NumRelocations;
    args.num_waitchks = NumWaitChecks;
    args.submit_version = hChannel->submit_version;
    args.cmdbufs = (uintptr_t)pCommandBufs;
    args.relocs = (uintptr_t)pRelocations;
    args.reloc_shifts = (uintptr_t)pRelocShifts;
    args.waitchks = (uintptr_t)pWaitChecks;
    args.waitbases = (uintptr_t)waitbases;
    args.fences = (uintptr_t)pSyncPointValue;

    do {
        ioctlret = ioctl(hChannel->fd, ioc, &args);
        if (ioctlret) {
            if (errno != EINTR) {
                NvOsDebugPrintf("NvRmChannelSubmit: "
                        "NvError_IoctlFailed with error code %d\n", errno);
                return NvError_IoctlFailed;
            }
        }
    } while (ioctlret);

    /* Keep compatibility with older kernels */
    if (pSyncPointValue && NumSyncPoints == 1)
        pSyncPointValue[StreamSyncPointIdx] = args.fence;

    return NvSuccess;
}

NvError
NvRmChannelSubmit(
    NvRmChannelHandle hChannel,
    const NvRmCommandBuffer *pCommandBufs, NvU32 NumCommandBufs,
    const NvRmChannelSubmitReloc *pRelocations, const NvU32 *pRelocShifts,
    NvU32 NumRelocations,
    const NvRmChannelWaitChecks *pWaitChecks, NvU32 NumWaitChecks,
    NvRmContextHandle hContext,
    const NvU32 *pContextExtra, NvU32 ContextExtraCount,
    NvBool NullKickoff, NvRmModuleID ModuleID,
    NvU32 StreamSyncPointIdx, NvU32 SyncPointWaitMask,
    NvU32 *pCtxChanged,
    NvU32 *pSyncPointValue,
    NvRmSyncPointDescriptor *SyncPointIncrs,
    NvU32 NumSyncPoints)
{
    struct nvhost_submit_args args;
    struct nvhost_syncpt_incr incr[NVRM_MAX_SYNCPOINTS_PER_SUBMIT];
    NvU32 waitbases[NVRM_MAX_SYNCPOINTS_PER_SUBMIT];
    int ioctlret;
    int ioc = NVHOST_IOCTL_CHANNEL_SUBMIT;
    NvU32 i;

    (void)hContext;
    (void)ModuleID;

    NV_ASSERT(!pContextExtra);
    NV_ASSERT(ContextExtraCount == 0);
    NV_ASSERT(!NullKickoff);
    NV_ASSERT(CheckSyncPointID(hChannel,
        SyncPointIncrs[StreamSyncPointIdx].SyncPointID));

    NvOsMemset(&args, 0, sizeof(args));
    args.num_syncpt_incrs = NumSyncPoints;
    args.syncpt_incrs = (uintptr_t)incr;
    for (i = 0; i < NumSyncPoints; ++i) {
        incr[i].syncpt_id = SyncPointIncrs[i].SyncPointID;
        incr[i].syncpt_incrs = SyncPointIncrs[i].Value;
        waitbases[i] = SyncPointIncrs[i].WaitBaseID;
    }
    if (pSyncPointValue)
        NvOsMemset(pSyncPointValue, 0,
            sizeof(*pSyncPointValue) * NumSyncPoints);
    args.num_cmdbufs = NumCommandBufs;
    args.num_relocs = NumRelocations;
    args.num_waitchks = NumWaitChecks;
    args.submit_version = hChannel->submit_version;
    args.cmdbufs = (uintptr_t)pCommandBufs;
    args.relocs = (uintptr_t)pRelocations;
    args.reloc_shifts = (uintptr_t)pRelocShifts;
    args.waitchks = (uintptr_t)pWaitChecks;
    args.waitbases = (uintptr_t)waitbases;
    args.fences = (uintptr_t)pSyncPointValue;

    do {
        ioctlret = ioctl(hChannel->fd, ioc, &args);
        if (ioctlret) {
            if (errno != EINTR)
                return NvRmChannelSubmitOld(hChannel,
                                            pCommandBufs, NumCommandBufs,
                                            pRelocations, pRelocShifts,
                                            NumRelocations, pWaitChecks,
                                            NumWaitChecks, hContext,
                                            pContextExtra, ContextExtraCount,
                                            NullKickoff, ModuleID,
                                            StreamSyncPointIdx,
                                            SyncPointWaitMask, pCtxChanged,
                                            pSyncPointValue, SyncPointIncrs,
                                            NumSyncPoints);
        }
    } while (ioctlret);

    /* Keep compatibility with older kernels */
    if (pSyncPointValue && NumSyncPoints == 1)
        pSyncPointValue[StreamSyncPointIdx] = args.fence;

    return NvSuccess;
}

NvError
NvRmChannelGetModuleSyncPoint(
    NvRmChannelHandle hChannel,
    NvRmModuleID Module,
    NvU32 Index,
    NvU32 *pSyncPointID)
{
    struct nvhost_get_param_arg arg;

    /* Ensure that this is not called for channels with dynamic syncpt id */
    if (hChannel->syncpt_type == NvRmChannelSyncptType_Dynamic)
            return NvError_InvalidState;

    /* special case display */
    if (NVRM_MODULE_ID_MODULE(Module) == NvRmModuleID_Display)
    {
        Index *= 2;
        Index += NVRM_MODULE_ID_INSTANCE(Module);
    }

    arg.param = Index;
    if (ioctl(hChannel->fd, NVHOST_IOCTL_CHANNEL_GET_SYNCPOINT, &arg) < 0) {
        struct nvhost_get_param_args args;
        if (ioctl(hChannel->fd, NVHOST_IOCTL_CHANNEL_GET_SYNCPOINTS, &args) < 0)
            return NvError_IoctlFailed;
        *pSyncPointID = IndexOfNthSetBit(args.value, Index);
    } else
        *pSyncPointID = arg.value;

    /* Set channel syncpt type as static */
    hChannel->syncpt_type = NvRmChannelSyncptType_Static;
    return (*pSyncPointID != (NvU32)-1) ? NvSuccess : NvError_BadParameter;
}

NvS32
NvRmChannelGetModuleTimedout(
    NvRmChannelHandle hChannel,
    NvRmModuleID Module,
    NvU32 Index)
{
    struct nvhost_get_param_args args;

    args.value = 0;
    if (ioctl(hChannel->fd, NVHOST_IOCTL_CHANNEL_GET_TIMEDOUT, &args) < 0)
        return -1;

    return args.value;
}

NvU32 NvRmChannelGetModuleWaitBase(
    NvRmChannelHandle hChannel,
    NvRmModuleID Module,
    NvU32 Index)
{
    struct nvhost_get_param_arg arg;
    NvU32 id;

    arg.param = Index;
    if (ioctl(hChannel->fd, NVHOST_IOCTL_CHANNEL_GET_WAITBASE, &arg) < 0) {
        struct nvhost_get_param_args args;
        if (ioctl(hChannel->fd, NVHOST_IOCTL_CHANNEL_GET_WAITBASES, &args) < 0)
            return 0;
        id = IndexOfNthSetBit(args.value, Index);
    } else
        id = arg.value;

    return id;
}

NvError
NvRmChannelGetModuleClockRate(
    NvRmChannelHandle hChannel,
    NvRmModuleID Module,
    NvU32 *Rate)
{
    struct nvhost_clk_rate_args args;
    args.moduleid = (Module << NVHOST_MODULE_ID_BIT_POS);
    if (ioctl(hChannel->fd, NVHOST_IOCTL_CHANNEL_GET_CLK_RATE, &args) < 0) {
        if(errno != EINTR) {
            NvOsDebugPrintf("NvRmChannelGetModuleClockRate: "
                    "NvError_IoctlFailed with error code %d\n", errno);
            return NvError_IoctlFailed;
        }
    }
    *Rate = args.rate / 1000;
    return NvSuccess;
}

NvError
NvRmChannelSetModuleClockRate(
    NvRmChannelHandle hChannel,
    NvRmModuleID Module,
    NvU32 Rate)
{
    struct nvhost_clk_rate_args args;
    args.moduleid = (Module << NVHOST_MODULE_ID_BIT_POS)
                    | (NVHOST_CLOCK << NVHOST_CLOCK_ATTR_BIT_POS);
    args.rate = Rate * 1000;
    if (ioctl(hChannel->fd, NVHOST_IOCTL_CHANNEL_SET_CLK_RATE, &args) < 0) {
        if(errno != EINTR) {
            NvOsDebugPrintf("NvRmChannelSetModuleClockRate: "
                    "NvError_IoctlFailed with error code %d\n", errno);
            return NvError_IoctlFailed;
        }
    }
    return NvSuccess;
}

NvError
NvRmChannelSetModuleBandwidth(
    NvRmChannelHandle hChannel,
    NvRmModuleID Module,
    NvU32 BW)
{
    struct nvhost_clk_rate_args args;
    args.moduleid = (Module << NVHOST_MODULE_ID_BIT_POS)
                    | (NVHOST_BW << NVHOST_CLOCK_ATTR_BIT_POS);
    args.rate = BW * 1000;
    if (ioctl(hChannel->fd, NVHOST_IOCTL_CHANNEL_SET_CLK_RATE, &args) < 0) {
        if(errno != EINTR) {
            NvOsDebugPrintf("NvRmChannelSetModuleBandwidth: "
                    "NvError_IoctlFailed with error code %d\n", errno);
            return NvError_IoctlFailed;
        }
    }
    return NvSuccess;
}

NvError
NvRmChannelSetPriority(
    NvRmChannelHandle hChannel,
    NvU32 Priority,
    NvU32 SyncPointIndex,
    NvU32 WaitBaseIndex,
    NvU32 *SyncPointID,
    NvU32 *WaitBase)
{
    struct nvhost_set_priority_args args;
    args.priority = Priority;

    /* Ensure that caller hasn't retrieved static sync point id */
    if (SyncPointID && hChannel->syncpt_type == NvRmChannelSyncptType_Static)
        return NvError_InvalidState;

    if (ioctl(hChannel->fd, NVHOST_IOCTL_CHANNEL_SET_PRIORITY, &args) < 0) {
        NvOsDebugPrintf("NvRmChannelSetPriority: "
                "NvError_IoctlFailed with error code %d\n", errno);
        return NvError_IoctlFailed;
    }

    if (SyncPointID) {
        NvError err;

        /* Allow using NvRmChannelGetModuleSyncPoint() inside this function. */
        hChannel->syncpt_type = NvRmChannelSyncptType_Unknown;

        /* Module id doesn't matter as long as it's not display */
        err = NvRmChannelGetModuleSyncPoint(hChannel,
            NvRmModuleID_GraphicsHost, SyncPointIndex, SyncPointID);
        /* If we manage to set priority, but not get the new sync point, only
         * safe thing left to do is to bail out. Otherwise kernel and user
         * space are going to have mismatch on sync point id. */
        if (err != NvSuccess)
            NV_ASSERT(!"Could not get new sync point");

        /* Assign channel type as dynamic */
        hChannel->syncpt_type = NvRmChannelSyncptType_Dynamic;
    }

    if (WaitBase) {
        /* Module id doesn't matter as long as it's not display */
        *WaitBase = NvRmChannelGetModuleWaitBase(hChannel,
            NvRmModuleID_GraphicsHost, WaitBaseIndex);
        /* If we manage to set priority, but not get the new wait base, only
         * safe thing left to do is to bail out. Otherwise kernel and user
         * space are going to have mismatch on wait base id. */
        NV_ASSERT(*WaitBase != 0);
    }

    return NvSuccess;
}

enum KernelModuleId {
    KernelModuleId_Display_0 = 0,
    KernelModuleId_Display_1,
    KernelModuleId_VI,
    KernelModuleId_ISP,
    KernelModuleId_MPE,
    KernelModuleId_MSENC,
    KernelModuleId_TSEC,
    KernelModuleId_INVALD = 0xFFFFFFFF,
};

static NvU32 GetModuleIndex(NvRmModuleID id)
{
    NvU32 mod = NVRM_MODULE_ID_MODULE(id);
    NvU32 idx = NVRM_MODULE_ID_INSTANCE(id);

    switch (mod)
    {
    case NvRmModuleID_Display:
        if (idx == 0) return KernelModuleId_Display_0;
        if (idx == 1) return KernelModuleId_Display_1;
        break;
    case NvRmModuleID_Vi:
        if (idx == 0 || idx == 1) return NVRM_MODULE_ID(KernelModuleId_VI, idx);
        break;
    case NvRmModuleID_Isp:
        if (idx == 0 || idx == 1) return NVRM_MODULE_ID(KernelModuleId_ISP, idx);
        break;
    case NvRmModuleID_Mpe:
        if (idx == 0) return KernelModuleId_MPE;
        break;
    case NvRmModuleID_Vic:
        if (idx == 0) return KernelModuleId_MSENC;
        break;
    case NvRmModuleID_MSENC:
        if (idx == 0) return KernelModuleId_MSENC;
        break;
    case NvRmModuleID_TSEC:
        if (idx == 0) return KernelModuleId_TSEC;
        break;
    default:
        break;
    }

    return KernelModuleId_INVALD;
}

static NvError ChannelRegReadWriteOld(NvRmChannelHandle ch,
    NvRmModuleID id,
    NvU32 num_offsets,
    NvU32 block_size,
    const NvU32 *offsets,
    NvU32 *values,
    NvBool write);
static NvError ChannelRegReadWriteOld(NvRmChannelHandle ch,
    NvRmModuleID id,
    NvU32 num_offsets,
    NvU32 block_size,
    const NvU32 *offsets,
    NvU32 *values,
    NvBool write)
{
    struct nvhost32_ctrl_module_regrdwr_args args;
    int err = 0;

    args.id = GetModuleIndex(id);
    args.num_offsets = num_offsets;
    args.block_size = block_size;
    args.offsets = (uintptr_t)offsets;
    args.values = (uintptr_t)values;
    args.write = write ? 1 : 0;

    err = ioctl(ch->fd, NVHOST32_IOCTL_CHANNEL_MODULE_REGRDWR, &args);
    if (err < 0)
        return NvError_IoctlFailed;

    return NvSuccess;
}

static NvError ChannelRegReadWrite(NvRmChannelHandle ch,
    NvRmModuleID id,
    NvU32 num_offsets,
    NvU32 block_size,
    const NvU32 *offsets,
    NvU32 *values,
    NvBool write)
{
    struct nvhost_ctrl_module_regrdwr_args args;
    int err = 0;

    args.id = GetModuleIndex(id);
    args.num_offsets = num_offsets;
    args.block_size = block_size;
    args.offsets = (uintptr_t)offsets;
    args.values = (uintptr_t)values;
    args.write = write ? 1 : 0;

    err = ioctl(ch->fd, NVHOST_IOCTL_CHANNEL_MODULE_REGRDWR, &args);
    if (err)
        return ChannelRegReadWriteOld(ch, id, num_offsets, block_size, offsets,
                                      values, write);
    return NvSuccess;
}

NvError
NvRmChannelRegRd(
    NvRmDeviceHandle device,
    NvRmChannelHandle ch,
    NvRmModuleID id,
    NvU32 num,
    const NvU32 *offsets,
    NvU32 *values)
{
    (void)device;
    return ChannelRegReadWrite(ch, id, num, 4, offsets,
        values, NV_FALSE);
}

NvError
NvRmChannelRegWr(
    NvRmDeviceHandle device,
    NvRmChannelHandle ch,
    NvRmModuleID id,
    NvU32 num,
    const NvU32 *offsets,
    const NvU32 *values)
{
    (void)device;
    return ChannelRegReadWrite(ch, id, num, 4, offsets,
        (NvU32 *)values, NV_TRUE);
}

NvError
NvRmChannelBlockRegRd(
    NvRmDeviceHandle device,
    NvRmChannelHandle ch,
    NvRmModuleID id,
    NvU32 num,
    NvU32 offset,
    NvU32 *values)
{
    (void)device;
    return ChannelRegReadWrite(ch, id, 1, num * 4, &offset,
        values, NV_FALSE);
}

NvError
NvRmChannelBlockRegWr(
    NvRmDeviceHandle device,
    NvRmChannelHandle ch,
    NvRmModuleID id,
    NvU32 num,
    NvU32 offset,
    NvU32 *values)
{
    (void)device;
    return ChannelRegReadWrite(ch, id, 1, num * 4, &offset,
        (NvU32 *)values, NV_TRUE);
}

NvU32
NvRmChannelSyncPointRead(
    NvRmDeviceHandle hDevice,
    NvU32 SyncPointID)
{
    struct nvhost_ctrl_syncpt_read_args args;

    (void)hDevice;

    NvOsMemset(&args, 0, sizeof(args));
    args.id = SyncPointID;
    if (ioctl(s_CtrlDev, NVHOST_IOCTL_CTRL_SYNCPT_READ, &args) < 0)
        NV_ASSERT(!"Can't handle ioctl failure");
    return args.value;
}

/* Read contents of a sync point sysfs file */
static NvError ReadFile(char *Name, char *Buffer, int Size)
{
    NvError err;
    NvOsFileHandle file;
    size_t count = 0;

    /* open file */
    err = NvOsFopen(Name, NVOS_OPEN_READ, &file);
    if(err) {
        NvOsDebugPrintf("Couldn't open %s\n", Name);
        return err;
    }

    /* read the contents of the file */
    err = NvOsFread(file, Buffer, Size-1, &count);
    /* End of file means the number was shorter than Size - that's ok */
    if (err == NvError_EndOfFile)
        err = NvSuccess;

    /*  add zero to make it a string */
    Buffer[count] = '\0';

    NvOsFclose(file);
    return err;
}

/* Name of the sysfs file for capability entries */
#define SYNCPT_NUM_PTS_NODE     "/sys/devices/platform/host1x/capabilities/num_pts"
#define SYNCPT_NUM_MLOCKS_NODE  "/sys/devices/platform/host1x/capabilities/num_mlocks"
#define SYNCPT_NUM_BASES_NODE   "/sys/devices/platform/host1x/capabilities/num_bases"

/* Name of the sysfs file for the syncpt max */
#define MAX_NAME_TEMPLATE "/sys/bus/nvhost/devices/host1x/syncpt/%d/max"
/* Length of UINT_MAX in characters */
#define MAX_INT_LENGTH 10

NvError
NvRmChannelNumSyncPoints(NvU32 *Value)
{
    NvError err;
    char valueStr[MAX_INT_LENGTH];

    err = ReadFile(SYNCPT_NUM_PTS_NODE, valueStr, MAX_INT_LENGTH);
    if (!err)
        *Value = atoi(valueStr);

    return err;
}

NvError
NvRmChannelNumMutexes(NvU32 *Value)
{
    NvError err;
    char valueStr[MAX_INT_LENGTH];

    err = ReadFile(SYNCPT_NUM_MLOCKS_NODE, valueStr, MAX_INT_LENGTH);
    if (!err)
        *Value = atoi(valueStr);

    return err;
}

NvError
NvRmChannelNumWaitBases(NvU32 *Value)
{
    NvError err;
    char valueStr[MAX_INT_LENGTH];

    err = ReadFile(SYNCPT_NUM_BASES_NODE, valueStr, MAX_INT_LENGTH);
    if (!err)
        *Value = atoi(valueStr);

    return err;
}

NvError
NvRmChannelSyncPointReadMax(
    NvRmDeviceHandle hDevice,
    NvU32 SyncPointID,
    NvU32 *Value)
{
    NvError err;
    /* Allocate space for path - %d holds two digits, so count that out */
    char name[sizeof(MAX_NAME_TEMPLATE)+MAX_INT_LENGTH-2];
    char valueStr[MAX_INT_LENGTH];
    const char *tmpl = MAX_NAME_TEMPLATE;
    struct nvhost_ctrl_syncpt_read_args args;

    /* attempt to use the new ioctl interface */
    args.id = SyncPointID;
    if (ioctl(s_CtrlDev, NVHOST_IOCTL_CTRL_SYNCPT_READ_MAX, &args) == 0) {
        *Value = args.value;
        return NvSuccess;
    }

    /* fall back to the old sysfs interface */
    NvOsSnprintf(name, sizeof(name), tmpl, SyncPointID);
    err = ReadFile(name, valueStr, MAX_INT_LENGTH);
    if (!err)
        *Value = atoi(valueStr);

    return err;
}

NvError
NvRmFenceTrigger(
    NvRmDeviceHandle hDevice,
    const NvRmFence *Fence)
{
    struct nvhost_ctrl_syncpt_incr_args args;
    NvU32 cur = NvRmChannelSyncPointRead(hDevice, Fence->SyncPointID);
    NvError err = NvSuccess;

    /* Can trigger only fence that is eligble (=next sync point val) */
    if (Fence->Value == cur+1) {
        args.id = Fence->SyncPointID;
        if (ioctl(s_CtrlDev, NVHOST_IOCTL_CTRL_SYNCPT_INCR, &args) < 0)
            err = NvError_IoctlFailed;
    } else {
        err = NvError_InvalidState;
    }

    return err;
}

void
NvRmChannelSyncPointIncr(
    NvRmDeviceHandle hDevice,
    NvU32 SyncPointID)
{
    struct nvhost_ctrl_syncpt_incr_args args;

    (void)hDevice;

    args.id = SyncPointID;
    if (ioctl(s_CtrlDev, NVHOST_IOCTL_CTRL_SYNCPT_INCR, &args) < 0)
        NV_ASSERT(!"Can't handle ioctl failure");
}

typedef struct
{
    NvU32 SyncPointID;
    NvU32 Threshold;
    NvOsSemaphoreHandle hSema;
} SyncPointSignalArgs;

static void* SyncPointSignalSemaphoreThread(void* vargs)
{
    SyncPointSignalArgs* args = (SyncPointSignalArgs*)vargs;
    NvRmChannelSyncPointWait(NULL, args->SyncPointID, args->Threshold, NULL);
    NvOsSemaphoreSignal(args->hSema);
    NvOsSemaphoreDestroy(args->hSema);
    NvOsFree(args);
    pthread_exit(NULL);
    return NULL;
}

NvBool
NvRmChannelSyncPointSignalSemaphore(
    NvRmDeviceHandle hDevice,
    NvU32 SyncPointID,
    NvU32 Threshold,
    NvOsSemaphoreHandle hSema)
{
    SyncPointSignalArgs* args;
    pthread_t thread;
    pthread_attr_t tattr;
    NvU32 val;

    val = NvRmChannelSyncPointRead(hDevice, SyncPointID);
    if (NvSchedValueWrappingComparison(val, Threshold))
    {
        return NV_TRUE;
    }

    args = NvOsAlloc(sizeof(SyncPointSignalArgs));
    if (!args)
        goto poll;

    args->SyncPointID = SyncPointID;
    args->Threshold = Threshold;
    (void)NvOsSemaphoreClone(hSema, &args->hSema);

    pthread_attr_init(&tattr);
    pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
    if (pthread_create(&thread, &tattr, SyncPointSignalSemaphoreThread, args))
    {
        NvOsSemaphoreDestroy(args->hSema);
        NvOsFree(args);
        pthread_attr_destroy(&tattr);
        goto poll;
    }
    pthread_attr_destroy(&tattr);
    return NV_FALSE;

 poll:
    do
    {
        val = NvRmChannelSyncPointRead(hDevice, SyncPointID);
    } while ((NvS32)(val - Threshold) < 0);
    return NV_TRUE;
}

NvBool
NvRmFenceSignalSemaphore(
    NvRmDeviceHandle hDevice,
    const NvRmFence *Fence,
    NvOsSemaphoreHandle hSema)
{
    SyncPointSignalArgs* args;
    pthread_t thread;
    pthread_attr_t tattr;
    NvU32 val;

    val = NvRmChannelSyncPointRead(hDevice, Fence->SyncPointID);
    if (NvSchedValueWrappingComparison(val, Fence->Value))
    {
        NvOsSemaphoreSignal(hSema);
        return NV_TRUE;
    }

    args = NvOsAlloc(sizeof(SyncPointSignalArgs));
    if (!args)
        goto poll;

    args->SyncPointID = Fence->SyncPointID;
    args->Threshold = Fence->Value;
    (void)NvOsSemaphoreClone(hSema, &args->hSema);

    pthread_attr_init(&tattr);
    pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
    if (pthread_create(&thread, &tattr, SyncPointSignalSemaphoreThread, args))
    {
        NvOsSemaphoreDestroy(args->hSema);
        NvOsFree(args);
        pthread_attr_destroy(&tattr);
        goto poll;
    }
    pthread_attr_destroy(&tattr);
    return NV_FALSE;

 poll:
    do
    {
        val = NvRmChannelSyncPointRead(hDevice, Fence->SyncPointID);
    } while ((NvS32)(val - Fence->Value) < 0);
    return NV_TRUE;
}


void
NvRmChannelSyncPointWait(
    NvRmDeviceHandle hDevice,
    NvU32 SyncPointID,
    NvU32 Threshold,
    NvOsSemaphoreHandle hSema)
{
    (void)NvRmChannelSyncPointWaitTimeout(hDevice, SyncPointID, Threshold, hSema, NVHOST_NO_TIMEOUT);
}

NvError
NvRmChannelSyncPointWaitTimeout(
    NvRmDeviceHandle hDevice,
    NvU32 SyncPointID,
    NvU32 Threshold,
    NvOsSemaphoreHandle hSema,
    NvU32 Timeout)
{
    return NvRmChannelSyncPointWaitexTimeout(hDevice, SyncPointID, Threshold, hSema, Timeout, NULL);
}

NvError
NvRmChannelSyncPointWaitexTimeout(
    NvRmDeviceHandle hDevice,
    NvU32 SyncPointID,
    NvU32 Threshold,
    NvOsSemaphoreHandle hSema,
    NvU32 Timeout,
    NvU32 *Actual)
{
    struct nvhost_ctrl_syncpt_waitex_args args = {0, 0, 0, 0};
    int requesttype = NVHOST_IOCTL_CTRL_SYNCPT_WAITEX;

    (void)hDevice;
    (void)hSema;

    args.id = SyncPointID;
    args.thresh = Threshold;
    args.timeout = Timeout;

    while (ioctl(s_CtrlDev, requesttype, &args) < 0)
    {
        if ( (errno == ENOTTY) || (errno == EFAULT) )
            requesttype = NVHOST_IOCTL_CTRL_SYNCPT_WAIT;
        else if (errno != EINTR)
            return errno == EAGAIN ? NvError_Timeout : NvError_InsufficientMemory;
    }

    if (requesttype == NVHOST_IOCTL_CTRL_SYNCPT_WAIT && Actual)
    {
        args.value = NvRmChannelSyncPointRead(hDevice, SyncPointID);
    }

    if (Actual)
    {
        *Actual = args.value;
    }
    return NvSuccess;
}

NvError
NvRmChannelSyncPointWaitmexTimeout(
    NvRmDeviceHandle hDevice,
    NvU32 SyncPointID,
    NvU32 Threshold,
    NvOsSemaphoreHandle hSema,
    NvU32 Timeout,
    NvU32 *Actual,
    NvRmTimeval *tv)
{
    struct nvhost_ctrl_syncpt_waitmex_args args;

    (void)hDevice;
    (void)hSema;

    NvOsMemset(&args, 0, sizeof(args));
    args.id = SyncPointID;
    args.thresh = Threshold;
    args.timeout = Timeout;

    while (ioctl(s_CtrlDev, NVHOST_IOCTL_CTRL_SYNCPT_WAITMEX, &args) < 0)
        switch (errno) {
        case EINTR:
            continue;
        case ENOTTY:
        case EFAULT:
            return NvError_NotImplemented;
        case EAGAIN:
            return NvError_Timeout;
        default:
            return NvError_InsufficientMemory;
        }

    if (Actual)
        *Actual = args.value;

    if (tv) {
        tv->sec = args.tv_sec;
        tv->nsec = args.tv_nsec;
    }
    return NvSuccess;
}

NvError
NvRmFenceWait(
    NvRmDeviceHandle hDevice,
    const NvRmFence *Fence,
    NvU32 Timeout)
{
    struct nvhost_ctrl_syncpt_waitex_args args = {0, 0, 0, 0};
    int requesttype = NVHOST_IOCTL_CTRL_SYNCPT_WAITEX;

    args.id = Fence->SyncPointID;
    args.thresh = Fence->Value;
    args.timeout = Timeout;

    while (ioctl(s_CtrlDev, requesttype, &args) < 0)
    {
        if ( (errno == ENOTTY) || (errno == EFAULT) )
            requesttype = NVHOST_IOCTL_CTRL_SYNCPT_WAIT;
        else if (errno != EINTR)
            return errno == EAGAIN ? NvError_Timeout : NvError_InsufficientMemory;
    }

    return NvSuccess;
}

// Assert that NvRmFence and nvhost_ctrl_sync_fence_info have the same layout
NV_CT_ASSERT(NV_OFFSETOF(NvRmFence, SyncPointID) == NV_OFFSETOF(struct nvhost_ctrl_sync_fence_info, id));
NV_CT_ASSERT(NV_OFFSETOF(NvRmFence, Value) == NV_OFFSETOF(struct nvhost_ctrl_sync_fence_info, thresh));
NV_CT_ASSERT(sizeof(NvRmFence) == sizeof(struct nvhost_ctrl_sync_fence_info));

NvError
NvRmFencePutToFileOld(
    const char *Name,
    const NvRmFence *Fences,
    NvU32 NumFences,
    NvS32 *Fd);
NvError
NvRmFencePutToFileOld(
    const char *Name,
    const NvRmFence *Fences,
    NvU32 NumFences,
    NvS32 *Fd)
{
#if !defined(ANDROID)
    return NvError_NotSupported;
#else
    struct nvhost32_ctrl_sync_fence_create_args args;
    int requesttype = NVHOST32_IOCTL_CTRL_SYNC_FENCE_CREATE;

    args.num_pts = NumFences;
    args.pts = (__u64)(uintptr_t)Fences;
    args.name = (__u64)(uintptr_t)Name;

    if (ioctl(s_CtrlDev, requesttype, &args) < 0)
    {
        NvOsDebugPrintf("sync_fence_create ioctl failed with %d\n", errno);
        *Fd = -errno;
        return NvError_IoctlFailed;
    }

    *Fd = args.fence_fd;
    return NvSuccess;
#endif
}

NvError
NvRmFencePutToFile(
    const char *Name,
    const NvRmFence *Fences,
    NvU32 NumFences,
    NvS32 *Fd)
{
#if !defined(ANDROID)
    return NvError_NotSupported;
#else
    struct nvhost_ctrl_sync_fence_create_args args;
    int requesttype = NVHOST_IOCTL_CTRL_SYNC_FENCE_CREATE;

    args.num_pts = NumFences;
    args.pts = (__u64)(uintptr_t)Fences;
    args.name = (__u64)(uintptr_t)Name;

    if (ioctl(s_CtrlDev, requesttype, &args) < 0)
        return NvRmFencePutToFileOld(Name, Fences, NumFences, Fd);

    *Fd = args.fence_fd;
    return NvSuccess;
#endif
}

NvError
NvRmFenceGetFromFile(
    NvS32 Fd,
    NvRmFence *Fences,
    NvU32 *NumFences)
{
#if !defined(ANDROID)
    return NvError_NotSupported;
#else
    struct sync_fence_info_data *info;
    struct sync_pt_info *ptInfo = NULL;
    NvError ret = NvSuccess;
    NvU32 i = 0;

    NV_ASSERT(NumFences);

    info = sync_fence_info(Fd);
    if (!info)
    {
        NvOsDebugPrintf("sync_fence_info failed with %d\n", errno);
        // sync_fence_info is just a wrapper to an ioctl.
        return NvError_IoctlFailed;
    }

    while ((ptInfo = sync_pt_info(info, ptInfo)))
    {
        struct nvhost_ctrl_sync_fence_info driverInfo;
        // Make a copy to avoid cast-alignment warning
        NvOsMemcpy(&driverInfo, &ptInfo->driver_data, sizeof(driverInfo));
        if (Fences)
        {
            if (i == *NumFences)
            {
                // Too short fence array was provided
                ret = NvError_BadParameter;
                goto out;
            }
            Fences[i].SyncPointID = driverInfo.id;
            Fences[i].Value = driverInfo.thresh;
        }
        i++;
    }
    *NumFences = i;

out:
    sync_fence_info_free(info);
    return ret;
#endif
}

NvU32
NvRmChannelGetModuleMutex(
    NvRmModuleID Module,
    NvU32 Index)
{
    NvRmChannelHandle ch;
    NvError err;
    struct nvhost_get_param_arg arg;
    NvU32 ret;

    err = NvRmChannelOpen(NULL, &ch, 1, &Module);
    if (err)
        return 0xffffffff;

    arg.param = Index;
    if (ioctl(ch->fd, NVHOST_IOCTL_CHANNEL_GET_MODMUTEX, &arg) < 0) {
        struct nvhost_get_param_args args;
        if (ioctl(ch->fd, NVHOST_IOCTL_CHANNEL_GET_MODMUTEXES, &args) < 0) {
            ret = 0xffffffff;
        } else
            ret = IndexOfNthSetBit(args.value, Index);
    } else {
        ret = arg.value;
    }

    NvRmChannelClose(ch);

    return ret;
}

static void ModuleMutexLock(NvU32 Index, int lock)
{
    struct nvhost_ctrl_module_mutex_args args;

    args.id = Index;
    args.lock = lock;

    while (ioctl(s_CtrlDev, NVHOST_IOCTL_CTRL_MODULE_MUTEX, &args) < 0)
    {
        NV_ASSERT(errno == EBUSY);
    }
}

void
NvRmChannelModuleMutexLock(
    NvRmDeviceHandle hDevice,
    NvU32 Index)
{
    (void)hDevice;
    ModuleMutexLock(Index, 1);
}

void
NvRmChannelModuleMutexUnlock(
    NvRmDeviceHandle hDevice,
    NvU32 Index)
{
    (void)hDevice;
    ModuleMutexLock(Index, 0);
}


static NvError ModuleRegReadWriteOld(
    NvRmModuleID id,
    NvU32 num_offsets,
    NvU32 block_size,
    const NvU32 *offsets,
    NvU32 *values,
    NvBool write);
static NvError ModuleRegReadWriteOld(
    NvRmModuleID id,
    NvU32 num_offsets,
    NvU32 block_size,
    const NvU32 *offsets,
    NvU32 *values,
    NvBool write)
{
    struct nvhost32_ctrl_module_regrdwr_args args;
    int err = 0;

    args.id = GetModuleIndex(id);
    args.num_offsets = num_offsets;
    args.block_size = block_size;
    args.offsets = (uintptr_t)offsets;
    args.values = (uintptr_t)values;
    args.write = write ? 1 : 0;

    err = ioctl(s_CtrlDev, NVHOST32_IOCTL_CTRL_MODULE_REGRDWR, &args);
    if (err < 0)
        return NvError_IoctlFailed;

    return NvSuccess;
}
static NvError ModuleRegReadWrite(
    NvRmModuleID id,
    NvU32 num_offsets,
    NvU32 block_size,
    const NvU32 *offsets,
    NvU32 *values,
    NvBool write)
{
    struct nvhost_ctrl_module_regrdwr_args args;
    int err = 0;

    args.id = GetModuleIndex(id);
    args.num_offsets = num_offsets;
    args.block_size = block_size;
    args.offsets = (uintptr_t)offsets;
    args.values = (uintptr_t)values;
    args.write = write ? 1 : 0;

    err = ioctl(s_CtrlDev, NVHOST_IOCTL_CTRL_MODULE_REGRDWR, &args);
    if (err)
        return ModuleRegReadWriteOld(id, num_offsets, block_size, offsets,
                                     values, write);
    return NvSuccess;
}

NvError
NvRmHostModuleRegRd(
    NvRmDeviceHandle device,
    NvRmModuleID id,
    NvU32 num,
    const NvU32 *offsets,
    NvU32 *values)
{
    (void)device;
    return ModuleRegReadWrite(id, num, 4, offsets, values, NV_FALSE);
}

NvError
NvRmHostModuleRegWr(
    NvRmDeviceHandle device,
    NvRmModuleID id,
    NvU32 num,
    const NvU32 *offsets,
    const NvU32 *values)
{
    (void)device;
    return ModuleRegReadWrite(id, num, 4, offsets, (NvU32 *)values, NV_TRUE);
}

NvError
NvRmHostModuleBlockRegRd(
    NvRmDeviceHandle device,
    NvRmModuleID id,
    NvU32 num,
    NvU32 offset,
    NvU32 *values)
{
    (void)device;
    return ModuleRegReadWrite(id, 1, num*4, &offset, values, NV_FALSE);
}

NvError
NvRmHostModuleBlockRegWr(
    NvRmDeviceHandle device,
    NvRmModuleID id,
    NvU32 num,
    NvU32 offset,
    const NvU32 *values)
{
    (void)device;
    return ModuleRegReadWrite(id, 1, num*4, &offset, (NvU32 *)values, NV_TRUE);
}

/** Not needed or deprecated **/

NvError
NvRmContextAlloc(
    NvRmDeviceHandle hDevice,
    NvRmModuleID Module,
    NvRmContextHandle *phContext)
{
    *phContext = (NvRmContextHandle)1;
    return NvSuccess;
}

void
NvRmContextFree(
    NvRmContextHandle hContext)
{
}

NvError
NvRmChannelAlloc(
    NvRmDeviceHandle hDevice,
    NvRmChannelHandle *phChannel)
{
    NV_ASSERT(!"NvRmChannelAlloc not suppported");
    return NvError_NotSupported;
}

NvError
NvRmChannelSyncPointAlloc(
    NvRmDeviceHandle hDevice,
    NvU32 *pSyncPointID)
{
    return NvError_NotSupported;
}

void
NvRmChannelSyncPointFree(
    NvRmDeviceHandle hDevice,
    NvU32 SyncPointID)
{
}

NvError NvRmChannelRead3DRegister(
    NvRmChannelHandle hChannel,
    NvU32 Offset,
    NvU32* Value)
{
    struct nvhost_read_3d_reg_args args;

    args.value = 0;
    args.offset = Offset;

    if (ioctl(hChannel->fd, NVHOST_IOCTL_CHANNEL_READ_3D_REG, &args) < 0)
    {
        return NvError_NotSupported;
    } else {
        *Value = args.value;
        return NvSuccess;
    }
}

NvError NvRmChannelSetContextSwitch(
    NvRmChannelHandle hChannel,
    NvRmMemHandle hSave,
    NvU32 SaveWords,
    NvU32 SaveOffset,
    NvRmMemHandle hRestore,
    NvU32 RestoreWords,
    NvU32 RestoreOffset,
    NvU32 RelocOffset,
    NvU32 SyncptId,
    NvU32 WaitBase,
    NvU32 SaveIncrs,
    NvU32 RestoreIncrs)
{
    struct nvhost_set_ctxswitch_args args;
    struct nvhost_syncpt_incr save_incr;
    __u32 save_waitbase;
    struct nvhost_syncpt_incr restore_incr;
    __u32 restore_waitbase;
    struct nvhost_reloc reloc;
    struct nvhost_cmdbuf cmdbuf_save;
    struct nvhost_cmdbuf cmdbuf_restore;

    args.num_cmdbufs_save = 1;
    cmdbuf_save.mem = (NvU32)hSave;
    cmdbuf_save.words = SaveWords;
    cmdbuf_save.offset = SaveOffset;
    args.cmdbuf_save = &cmdbuf_save;
    args.num_save_incrs = 1;
    save_incr.syncpt_id = SyncptId;
    save_incr.syncpt_incrs = SaveIncrs;
    args.save_incrs = &save_incr;
    save_waitbase = WaitBase;
    args.save_waitbases = &save_waitbase;

    args.num_cmdbufs_restore = 1;
    cmdbuf_restore.mem = (NvU32)hRestore;
    cmdbuf_restore.words = RestoreWords;
    cmdbuf_restore.offset = RestoreOffset;
    args.cmdbuf_restore = &cmdbuf_restore;
    args.num_restore_incrs = 1;
    restore_incr.syncpt_id = SyncptId;
    restore_incr.syncpt_incrs = RestoreIncrs;
    args.restore_incrs = &restore_incr;
    restore_waitbase = WaitBase;
    args.restore_waitbases = &restore_waitbase;

    args.num_relocs = 1;
    reloc.cmdbuf_mem = (NvU32)hSave;
    reloc.cmdbuf_offset = RelocOffset;
    reloc.target = (NvU32)hRestore;
    reloc.target_offset = 0;
    args.relocs = &reloc;

    if (ioctl(hChannel->fd, NVHOST_IOCTL_CHANNEL_SET_CTXSWITCH, &args) < 0)
    {
        return NvError_IoctlFailed;
    } else {
        return NvSuccess;
    }

}
