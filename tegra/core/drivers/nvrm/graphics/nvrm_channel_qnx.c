/*
 * Copyright (c) 2011 - 2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvrm_channel_priv.h"
#include "nvrm_memmgr.h"
#include "nvos.h"
#include "nvassert.h"
#include "nvrm_sched.h"
#include <qnx/nvhost_devctl.h>
#include <qnx/nvmap_devctl.h>

#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>

struct NvRmChannelRec
{
    int fd;
    const char *device;
};

typedef struct
{
    NvU32 SyncPointID;
    NvU32 Threshold;
    NvOsSemaphoreHandle hSema;
} SyncPointSignalArgs;

static int s_CtrlDev = -1;

static void __attribute__ ((constructor)) nvrm_channel_init(void)
{
    s_CtrlDev = open(NVHOST_DEV_PREFIX "ctrl", O_RDWR);
}

static void __attribute__ ((destructor)) nvrm_channel_fini(void)
{
    if (s_CtrlDev != -1)
        close(s_CtrlDev);
}

static struct {
    NvU16 module;
    NvU16 instance;
    const char *dev_node;
} s_DeviceNodeMap[] = {
    { NvRmModuleID_Display, 0, NVHOST_DEV_PREFIX "display" },
    { NvRmModuleID_Display, 1, NVHOST_DEV_PREFIX "display" },
    { NvRmModuleID_2D,  0, NVHOST_DEV_PREFIX "gr2d" },
    { NvRmModuleID_3D,  0, NVHOST_DEV_PREFIX "gr3d" },
    { NvRmModuleID_Isp, 0, NVHOST_DEV_PREFIX "isp" },
    { NvRmModuleID_Vi,  0, NVHOST_DEV_PREFIX "vi" },
    { NvRmModuleID_Mpe, 0, NVHOST_DEV_PREFIX "mpe" },
    { NvRmModuleID_Dsi, 0, NVHOST_DEV_PREFIX "dsi" },
    { NvRmModuleID_MSENC, 0, NVHOST_DEV_PREFIX "msenc" },
    { NvRmModuleID_TSEC,  0, NVHOST_DEV_PREFIX "tsec" },
    { NvRmModuleID_Gpu, 0, NVHOST_DEV_PREFIX "gpu" },
    { NvRmModuleID_Vic, 0, NVHOST_DEV_PREFIX "vic" },
    { NvRmModuleID_Vi,  1, NVHOST_DEV_PREFIX "vi.1" },
    { NvRmModuleID_Isp, 1, NVHOST_DEV_PREFIX "isp.1" },
    { NvRmModuleID_Invalid, 0, NULL }
};

static const char* GetDeviceNode(NvRmModuleID mod)
{
    unsigned int i;
    NvU16 instance = NVRM_MODULE_ID_INSTANCE(mod);
    NvU16 module = NVRM_MODULE_ID_MODULE(mod);

    for (i=0; i < NV_ARRAY_SIZE(s_DeviceNodeMap); i++) {
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
    const NvRmModuleID *pModuleIDs)
{
    const char *node;
    NvRmChannelHandle ch;
    struct nvhost_set_nvmap_fd_args args;

    NV_ASSERT(pModuleIDs != NULL);
    NV_ASSERT(phChannel != NULL);

    if (!NumModules)
        return NvError_BadParameter;

    if (s_CtrlDev < 0)
    {
        s_CtrlDev = open(NVHOST_DEV_PREFIX "ctrl", O_RDWR);
        if (s_CtrlDev < 0)
        {
            NvOsDebugPrintf("%s: can't open " NVHOST_DEV_PREFIX "ctrl", __func__);
            return NvError_KernelDriverNotFound;
        }
    }

    node = GetDeviceNode(pModuleIDs[0]);
    if (node == NULL)
    {
        NvOsDebugPrintf("%s: unsupported module %d", __func__, pModuleIDs[0]);
        return NvError_BadParameter;
    }

    ch = NvOsAlloc(sizeof(*ch));
    if (ch == NULL) {
        NvOsDebugPrintf("%s: insufficient memory", __func__);
        return NvError_InsufficientMemory;
    }

    NvOsMemset(ch, 0, sizeof(*ch));
    ch->device = node;
    ch->fd = open(node, O_RDWR);
    if (ch->fd < 0)
    {
        NvOsFree(ch);
        NvOsDebugPrintf("%s: can't open channel %d (%s)", __func__, pModuleIDs[0], node);
        return NvError_KernelDriverNotFound;
    }

    args.fd = NvRm_MemmgrGetIoctlFile();
    if (devctl(ch->fd, NVHOST_DEVCTL_CHANNEL_SET_NVMAP_FD, &args,
               sizeof(args), NULL) != EOK)
    {
        NvRmChannelClose(ch);
        NvOsDebugPrintf("%s: can't set nvmap client for channel %d",
                        __func__, pModuleIDs[0]);
        return NvError_KernelDriverNotFound;
    }

    *phChannel = ch;
    return NvSuccess;
}

void NvRmChannelClose(NvRmChannelHandle ch)
{
    if (ch != NULL)
    {
        close(ch->fd);
        NvOsFree(ch);
    }
}

NvError
NvRmChannelSubmit(
    NvRmChannelHandle hChannel,
    const NvRmCommandBuffer *pCommandBufs, NvU32 NumCommandBufs,
    const NvRmChannelSubmitReloc *pRelocations, const NvU32 *RelocShifts,
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
    int ret;
    iov_t sv[5];
    iov_t rv[1];
    struct nvhost_submit_hdr_ext hdr;
    NvU32 flush;
    NvU32 SyncPointID = SyncPointIncrs[StreamSyncPointIdx].SyncPointID;
    NvU32 SyncPointsUsed = SyncPointIncrs[StreamSyncPointIdx].Value;

    NV_ASSERT(!pContextExtra);
    NV_ASSERT(ContextExtraCount == 0);
    NV_ASSERT(!NullKickoff);

    hdr.syncpt_id = SyncPointID;
    hdr.syncpt_incrs = SyncPointsUsed;
    hdr.num_cmdbufs = NumCommandBufs;
    hdr.num_relocs = NumRelocations;
    hdr.num_waitchks = NumWaitChecks;
    hdr.waitchk_mask = SyncPointWaitMask;
    hdr.submit_version = NVHOST_SUBMIT_VERSION_MAX_SUPPORTED;

    SETIOV(&sv[0], &hdr, sizeof(hdr));
    SETIOV(&sv[1], pCommandBufs, NumCommandBufs * sizeof(*pCommandBufs));
    SETIOV(&sv[2], pRelocations, NumRelocations * sizeof(*pRelocations));
    SETIOV(&sv[3], RelocShifts, NumRelocations * sizeof(*RelocShifts));
    SETIOV(&sv[4], pWaitChecks, NumWaitChecks * sizeof(*pWaitChecks));
    SETIOV(rv, &flush, sizeof(flush));

    ret = devctlv(hChannel->fd, NVHOST_DEVCTL_CHANNEL_FLUSH, NV_ARRAY_SIZE(sv),
                  NV_ARRAY_SIZE(rv), sv, rv, NULL);
    if (ret != EOK)
    {
        NvOsDebugPrintf("%s: devctl failure (%d)", __func__, ret);
        return NvError_IoctlFailed;
    }

    if (pSyncPointValue != NULL)
        *pSyncPointValue = flush;

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

    /* special case display */
    if (NVRM_MODULE_ID_MODULE(Module) == NvRmModuleID_Display)
    {
        Index *= 2;
        Index += NVRM_MODULE_ID_INSTANCE(Module);
    }

    arg.param = Index;
    if (devctl(hChannel->fd, NVHOST_DEVCTL_CHANNEL_GET_SYNCPOINT, &arg, sizeof(arg), NULL) != EOK)
        return NvError_IoctlFailed;

    *pSyncPointID = arg.value;
    return NvSuccess;
}

NvU32
NvRmChannelGetModuleWaitBase(
    NvRmChannelHandle hChannel,
    NvRmModuleID Module,
    NvU32 Index)
{
    struct nvhost_get_param_arg arg;

    arg.param = Index;
    if (devctl(hChannel->fd, NVHOST_DEVCTL_CHANNEL_GET_WAITBASE, &arg, sizeof(arg), NULL) != EOK)
    {
        NV_ASSERT(!"devctl failure");
        NvOsDebugPrintf("%s: devctl failure", __func__);
        return 0;
    }

    return arg.value;
}

NvError
NvRmChannelNumSyncPoints(NvU32 *Value)
{
    return NvError_NotImplemented;
}

NvError
NvRmChannelNumMutexes(NvU32 *Value)
{
    return NvError_NotImplemented;
}

NvError
NvRmChannelNumWaitBases(NvU32 *Value)
{
    return NvError_NotImplemented;
}

NvU32 NvRmChannelSyncPointRead(NvRmDeviceHandle hDevice, NvU32 SyncPointID)
{
    struct nvhost_ctrl_syncpt_read_args args;

    args.id = SyncPointID;
    if (devctl(s_CtrlDev, NVHOST_DEVCTL_CTRL_SYNCPT_READ, &args, sizeof(args), NULL) != EOK)
    {
        NV_ASSERT(!"devctl failure");
        NvOsDebugPrintf("%s: devctl failure", __func__);
        return 0;
    }

    return args.value;
}

NvError
NvRmChannelSyncPointReadMax(
    NvRmDeviceHandle hDevice,
    NvU32 SyncPointID,
    NvU32 *Value)
{
    struct nvhost_ctrl_syncpt_read_args args;

    NV_ASSERT(Value != NULL);

    args.id = SyncPointID;
    if (devctl(s_CtrlDev, NVHOST_DEVCTL_CTRL_SYNCPT_READ_MAX, &args, sizeof(args), NULL) != EOK)
    {
        NvOsDebugPrintf("%s: devctl failure", __func__);
        return NvError_IoctlFailed;
    }

    *Value = args.value;
    return NvSuccess;
}

NvError NvRmFenceTrigger(NvRmDeviceHandle hDevice, const NvRmFence *Fence)
{
    NvU32 cur;
    NvError err = NvSuccess;
    struct nvhost_ctrl_syncpt_incr_args args;

    NV_ASSERT(Fence != NULL);

    cur = NvRmChannelSyncPointRead(hDevice, Fence->SyncPointID);
    /* Can trigger only fence that is eligible (=next sync point val) */
    if (Fence->Value == cur + 1)
    {
        args.id = Fence->SyncPointID;
        if (devctl(s_CtrlDev, NVHOST_DEVCTL_CTRL_SYNCPT_INCR, &args, sizeof(args), NULL) != EOK)
            err = NvError_IoctlFailed;
    }
    else
        err = NvError_InvalidState;

    return err;
}

void NvRmChannelSyncPointIncr(NvRmDeviceHandle hDevice, NvU32 SyncPointID)
{
    struct nvhost_ctrl_syncpt_incr_args args;

    args.id = SyncPointID;
    if (devctl(s_CtrlDev, NVHOST_DEVCTL_CTRL_SYNCPT_INCR, &args, sizeof(args), NULL) != EOK)
    {
        NV_ASSERT(!"devctl failure");
        NvOsDebugPrintf("%s: devctl failure", __func__);
    }
}

static void *SyncPointSignalSemaphoreThread(void *vargs)
{
    SyncPointSignalArgs *args = (SyncPointSignalArgs *)vargs;
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
    NvU32 val;
    pthread_t thread;
    pthread_attr_t thr_attr;
    SyncPointSignalArgs *args;

    val = NvRmChannelSyncPointRead(hDevice, SyncPointID);
    if (NvSchedValueWrappingComparison(val, Threshold))
        return NV_TRUE;

    args = NvOsAlloc(sizeof(*args));
    if (args == NULL)
        goto poll;

    args->SyncPointID = SyncPointID;
    args->Threshold = Threshold;
    NvOsSemaphoreClone(hSema, &args->hSema);

    if (pthread_attr_init(&thr_attr) != EOK)
        goto fail;

    if (pthread_attr_setdetachstate(&thr_attr, PTHREAD_CREATE_DETACHED) != EOK)
    {
        pthread_attr_destroy(&thr_attr);
        goto fail;
    }

    if (pthread_create(&thread, &thr_attr, SyncPointSignalSemaphoreThread, args) != EOK)
    {
        pthread_attr_destroy(&thr_attr);
        goto fail;
    }

    return NV_FALSE;

fail:
    NvOsSemaphoreDestroy(args->hSema);
    NvOsFree(args);

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
    NvU32 val;
    pthread_t thread;
    pthread_attr_t thr_attr;
    SyncPointSignalArgs *args;

    NV_ASSERT(Fence != NULL);

    val = NvRmChannelSyncPointRead(hDevice, Fence->SyncPointID);
    if (NvSchedValueWrappingComparison(val, Fence->Value))
        return NV_TRUE;

    args = NvOsAlloc(sizeof(SyncPointSignalArgs));
    if (args == NULL)
        goto poll;

    args->SyncPointID = Fence->SyncPointID;
    args->Threshold = Fence->Value;
    NvOsSemaphoreClone(hSema, &args->hSema);

    if (pthread_attr_init(&thr_attr) != EOK)
        goto fail;

    if (pthread_attr_setdetachstate(&thr_attr, PTHREAD_CREATE_DETACHED) != EOK)
    {
        pthread_attr_destroy(&thr_attr);
        goto fail;
    }

    if (pthread_create(&thread, &thr_attr, SyncPointSignalSemaphoreThread, args) != EOK)
    {
        pthread_attr_destroy(&thr_attr);
        goto fail;
    }

    return NV_FALSE;

fail:
    NvOsSemaphoreDestroy(args->hSema);
    NvOsFree(args);

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
    NvRmChannelSyncPointWaitTimeout(hDevice, SyncPointID, Threshold, hSema, NVHOST_NO_TIMEOUT);
}

NvError
NvRmChannelSyncPointWaitTimeout(
    NvRmDeviceHandle hDevice,
    NvU32 SyncPointID,
    NvU32 Threshold,
    NvOsSemaphoreHandle hSema,
    NvU32 Timeout)
{
    return NvRmChannelSyncPointWaitexTimeout(hDevice, SyncPointID, Threshold, hSema,
                                             Timeout, NULL);
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
    int ret;
    struct nvhost_ctrl_syncpt_waitex_args args;

    args.id = SyncPointID;
    args.thresh = Threshold;
    args.timeout = Timeout;

    ret = devctl(s_CtrlDev, NVHOST_DEVCTL_CTRL_SYNCPT_WAITEX, &args, sizeof(args), NULL);
    if (ret != EOK)
        return (ret == EAGAIN) ? NvError_Timeout : NvError_InsufficientMemory;

    if (Actual != NULL)
        *Actual = args.value;
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
    return NvError_NotImplemented;
}

NvError
NvRmFenceWait(
    NvRmDeviceHandle hDevice,
    const NvRmFence *Fence,
    NvU32 Timeout)
{
    int ret;
    struct nvhost_ctrl_syncpt_waitex_args args;

    NV_ASSERT(Fence != NULL);

    args.id = Fence->SyncPointID;
    args.thresh = Fence->Value;
    args.timeout = Timeout;

    ret = devctl(s_CtrlDev, NVHOST_DEVCTL_CTRL_SYNCPT_WAITEX, &args, sizeof(args), NULL);
    if (ret != EOK)
        return (ret == EAGAIN) ? NvError_Timeout : NvError_InsufficientMemory;

    return NvSuccess;
}

NvError
NvRmFenceGetFromFile(
    NvS32 Fd,
    NvRmFence *Fences,
    NvU32 *NumFences)
{
    NV_ASSERT(!"NvRmFenceGetFromFile not supported");
    return NvError_NotSupported;
}

NvError
NvRmFencePutToFile(
    const char *Name,
    const NvRmFence *Fences,
    NvU32 NumFences,
    NvS32 *Fd)
{
    NV_ASSERT(!"NvRmFencePutToFile not supported");
    return NvError_NotSupported;
}

NvU32 NvRmChannelGetModuleMutex(NvRmModuleID Module, NvU32 Index)
{
    NvRmChannelHandle ch;
    NvError err;
    struct nvhost_get_param_arg arg;
    NvU32 ret;

    err = NvRmChannelOpen(NULL, &ch, 1, &Module);
    if (err)
        return 0xffffffff;

    arg.param = Index;
    if (devctl(ch->fd, NVHOST_DEVCTL_CHANNEL_GET_MODMUTEX, &arg, sizeof(arg), NULL) != EOK)
    {
        NvOsDebugPrintf("%s: devctl failure", __func__);
        ret = 0xffffffff;
    }
    else
    {
        ret = arg.value;
    }

    NvRmChannelClose(ch);

    return ret;
}

static void ModuleMutexLock(NvU32 Index, int lock)
{
    int ret;
    struct nvhost_ctrl_module_mutex_args args;

    args.id = Index;
    args.lock = lock;

    do
    {
        ret = devctl(s_CtrlDev, NVHOST_DEVCTL_CTRL_MODULE_MUTEX, &args, sizeof(args), NULL);
        if (ret != EOK && ret != EINTR) {
            NvOsDebugPrintf("%s: devctl failure", __func__);
            NV_ASSERT(0);
        }
    } while (ret == EINTR);
}

void NvRmChannelModuleMutexLock(NvRmDeviceHandle hDevice, NvU32 Index)
{
    ModuleMutexLock(Index, 1);
}

void NvRmChannelModuleMutexUnlock(NvRmDeviceHandle hDevice, NvU32 Index)
{
    ModuleMutexLock(Index, 0);
}

#if 0 /* not used? */
static NvU32 GetModuleIndex(NvRmModuleID id)
{
    NvU32 mod = NVRM_MODULE_ID_MODULE(id);
    NvU32 idx = NVRM_MODULE_ID_INSTANCE(id);

#if 0
    /* TODO: [ahatala 2010-07-02] find out if these are needed */
    MODULE_FUSE,
    MODULE_APB_MISC,
    MODULE_CLK_RESET,
#endif

    switch (mod)
    {
    case NvRmModuleID_Display:
        if (idx == 0) return 0;
        if (idx == 1) return 1;
        break;
    case NvRmModuleID_Vi:
        if (idx == 0) return 2;
        break;
    case NvRmModuleID_Isp:
        if (idx == 0) return 3;
        break;
    case NvRmModuleID_Mpe:
        if (idx == 0) return 4;
        break;
    default:
        break;
    }

    return (NvU32)-1;
}
#endif

static NvError ModuleRegReadWrite(
    NvRmModuleID id,
    NvU32 num_offsets,
    NvU32 block_size,
    const NvU32 *offsets,
    NvU32 *values,
    NvBool write)
{
    NV_ASSERT(!"ModuleRegReadWrite not implemented");
    return NvError_NotImplemented;
}

NvError
NvRmHostModuleRegRd(
    NvRmDeviceHandle device,
    NvRmModuleID id,
    NvU32 num,
    const NvU32 *offsets,
    NvU32 *values)
{
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
    return ModuleRegReadWrite(id, 1, num * 4, &offset, values, NV_FALSE);
}

NvError
NvRmHostModuleBlockRegWr(
    NvRmDeviceHandle device,
    NvRmModuleID id,
    NvU32 num,
    NvU32 offset,
    const NvU32 *values)
{
    return
        ModuleRegReadWrite(id, 1, num * 4, &offset, (NvU32 *)values, NV_TRUE);
}

/** not needed or deprecated **/

NvError NvRmContextAlloc(
    NvRmDeviceHandle hDevice,
    NvRmModuleID Module,
    NvRmContextHandle *phContext)
{
    *phContext = (NvRmContextHandle)1;
    return NvSuccess;
}

void NvRmContextFree(NvRmContextHandle hContext)
{
}

NvError NvRmChannelAlloc(NvRmDeviceHandle hDevice, NvRmChannelHandle *phChannel)
{
    NV_ASSERT(!"NvRmChannelAlloc not suppported");
    return NvError_NotSupported;
}

NvError NvRmChannelSyncPointAlloc(NvRmDeviceHandle hDevice, NvU32 *pSyncPointID)
{
    NV_ASSERT(!"NvRmChannelSyncPointAlloc not suppported");
    return NvError_NotSupported;
}

void NvRmChannelSyncPointFree(NvRmDeviceHandle hDevice, NvU32 SyncPointID)
{
}

NvError NvRmChannelSetSubmitTimeoutEx(NvRmChannelHandle ch, NvU32 SubmitTimeout,
                                        NvBool DisableDebugDump)
{
    return NvError_NotImplemented;
}

NvError NvRmChannelSetSubmitTimeout(NvRmChannelHandle ch, NvU32 SubmitTimeout)
{
    return NvError_NotImplemented;
}

NvS32 NvRmChannelGetModuleTimedout(
    NvRmChannelHandle hChannel,
    NvRmModuleID Module,
    NvU32 Index)
{
    return -1;
}

NvError
NvRmChannelRead3DRegister(
    NvRmChannelHandle hChannel,
    NvU32 Offset,
    NvU32 *Value)
{
    struct nvhost_read_3d_reg_args args;

    NV_ASSERT(Value != NULL);

    args.value = 0;
    args.offset = Offset;

    if (devctl(hChannel->fd, NVHOST_DEVCTL_CHANNEL_READ_3D_REG, &args, sizeof(args), NULL) == EOK)
    {
        *Value = args.value;
        return NvSuccess;
    }

    return NvError_NotSupported;
}

NvError NvRmChannelSetPriority(NvRmChannelHandle hChannel, NvU32 Priority,
    NvU32 SyncPointIndex, NvU32 WaitBaseIndex,
    NvU32 *SyncPointID, NvU32 *WaitBase)
{
    return NvError_NotSupported;
}

NvError
NvRmChannelGetModuleClockRate(
    NvRmChannelHandle hChannel,
    NvRmModuleID Module,
    NvU32 *Rate)
{
    return NvError_NotSupported;
}

NvError
NvRmChannelSetModuleClockRate(
    NvRmChannelHandle hChannel,
    NvRmModuleID Module,
    NvU32 Rate)
{
    return NvError_NotSupported;
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
    return NvError_NotSupported;
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
    return NvError_NotSupported;
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
    return NvError_NotSupported;
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
    return NvError_NotSupported;
}

NvError
NvRmChannelSetModuleBandwidth(
    NvRmChannelHandle hChannel,
    NvRmModuleID Module,
    NvU32 BW)
{
    return NvError_NotSupported;
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
    return NvError_NotSupported;
}
