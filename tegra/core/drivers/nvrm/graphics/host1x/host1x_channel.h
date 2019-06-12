/*
 * Copyright (c) 2007 - 2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_AP20_CHANNEL_H
#define INCLUDED_AP20_CHANNEL_H

#include "nvcommon.h"
#include "nvrm_rmctrace.h"
#include "nvrm_channel.h"
#include "nvrm_stream.h"
#include "nvrm_drf.h"
#include "nvrm_hardware_access.h"
#include "nvrm_power.h"
#include "project_arhost1x_sw_defs.h"

#ifdef __cplusplus
extern "C"
{
#endif  /* __cplusplus */
#define NVRM_SYNCPOINT_DISP0            (24)
#define NVRM_SYNCPOINT_DISP1            (25)
#define NVRM_SYNCPOINT_VBLANK0          (26)
#define NVRM_SYNCPOINT_VBLANK1          (27)
#define NVRM_SYNCPOINT_DSI              (31)

#define NVRM_SYNCPOINTS_RESERVED \
     ((1UL << NVRM_SYNCPOINT_DISP0)       | \
     (1UL << NVRM_SYNCPOINT_DISP1)       | \
     (1UL << NVRM_SYNCPOINT_VBLANK0)     | \
     (1UL << NVRM_SYNCPOINT_VBLANK1)     | \
     (1UL << NVRM_SYNCPOINT_DSI))

/* sync points that are wholly managed by the client */
#define NVRM_SYNCPOINTS_CLIENT_MANAGED \
    ((1UL << NVRM_SYNCPOINT_DISP0)  | \
     (1UL << NVRM_SYNCPOINT_DISP1)  | \
     (1UL << NVRM_SYNCPOINT_DSI))

#define NVRM_MODULEMUTEX_DISPLAYA       (6)
#define NVRM_MODULEMUTEX_DISPLAYB       (7)
#define NVRM_MODULEMUTEX_DSI            (9)

// 8 bytes per gather.  (This number does not include the final RESTART.)
#define NVRM_PUSHBUFFER_SIZE (NVRM_GATHER_QUEUE_SIZE * 8)

/* channel register read/write macros */
#define CHANNEL_REGR( ch, reg ) \
    NV_READ32( (ch)->ChannelRegs + ((HOST1X_CHANNEL_##reg##_0)/4) )

#define CHANNEL_REGW( ch, reg, data ) \
    NV_WRITE32( (ch)->ChannelRegs + ((HOST1X_CHANNEL_##reg##_0)/4), (data) )

/* macros used in the indirect read/write functions, because we don't have a
 * real channel handle here, just a pointer to the channel registers.
 */
#define CHANNEL_IND_REGR( pReg, RegName ) \
    NV_READ32( (pReg) + ((HOST1X_CHANNEL_##RegName##_0)/4) )

#define CHANNEL_IND_REGW( pReg, RegName, data ) \
    NV_WRITE32( (pReg) + ((HOST1X_CHANNEL_##RegName##_0)/4), (data) )

/* hw packet manipulation for the channel opcodes */
#define OP_DRF_DEF(o,f,c) \
    ((o##_##f##_##c) << NV_FIELD_SHIFT(o##_##f##_RANGE))

#define OP_DRF_NUM(o,f,n) \
    (((n)& NV_FIELD_MASK(o##_##f##_RANGE))<< NV_FIELD_SHIFT(o##_##f##_RANGE))

#define OP_DRF_VAL(o,f,v) \
    (((v)>> NV_FIELD_SHIFT(o##_##f##_RANGE))& NV_FIELD_MASK(o##_##f##_RANGE))

/* channel opcodes */
#define NVOPCODE_RESTART(address)                \
    (OP_DRF_DEF(HCFRESTART, OPCODE, RESTART) |  \
     OP_DRF_NUM(HCFRESTART, ADDRESS, address))

#define NVOPCODE_GATHER(offset,insert,type,count) \
    (OP_DRF_DEF(HCFGATHER, OPCODE, GATHER) |      \
     OP_DRF_NUM(HCFGATHER, OFFSET, offset) |      \
     OP_DRF_DEF(HCFGATHER, INSERT, insert) |      \
     OP_DRF_DEF(HCFGATHER, TYPE,   type)   |      \
     OP_DRF_NUM(HCFGATHER, COUNT,  count))

#define NVOPCODE_NOOP \
    (OP_DRF_DEF(HCFNONINCR, OPCODE, NONINCR)    | \
     OP_DRF_NUM(HCFNONINCR, OFFSET, 0)          | \
     OP_DRF_NUM(HCFNONINCR, COUNT, 0))

// Number of gathers we allow to be queued up per channel.  Must be a power of
// two.  Currently sized such that pushbuffer is 4KB (512*8B).
#define NVRM_GATHER_QUEUE_SIZE 512

typedef struct NvRmChannelRec
{
    /* The resource manager */
    NvRmDeviceHandle hDevice;

    /* semaphore that is signaled by channel implementation */
    NvOsSemaphoreHandle Semaphore;

    /* the hardware state */
    NvRmChHwHandle HwState;

    // FIXME: replace hContexts with a hashtable mapping engine to context
    NvRmContextHandle hContext3D;
    NvRmContextHandle hContextOther;
} NvRmChannel;

typedef struct
{
    NvRmMemHandle* HMem;
    NvU32* Phys;
    NvU32* Index;
    NvU32 Capacity;
    NvU32 Count;
} NvRmChPinBlock;

/*  The sync queue is a circular buffer of NvU32s interpreted as:
 *    0: SyncPointID
 *    1: SyncPointValue
 *    2: NumGathers (how many pushbuffer slots to free)
 *    3: NumHandles (NumRelocs + NumGathers)
 *    4..: NumHandles * NvRmMemHandle to unpin
 *
 *  If it is too small, we won't be able to queue up very many
 *  command buffers.  If it is too large, we waste memory.  It would be silly to
 *  set this to a value smaller than NVRM_GATHER_QUEUE_SIZE.
 *
 *  The size doesn't have to be a power of 2, it just is.
 */
#define NVRM_SYNC_QUEUE_SIZE 8192

typedef enum
{
    NvRmChSyncQueueEvent_Nothing = 1,
    NvRmChSyncQueueEvent_Empty,
    NvRmChSyncQueueEvent_SyncQueue,
    NvRmChSyncQueueEvent_PushBuffer,

    NvRmChSyncQueueEvent_Force32 = 0x7fffffff
} NvRmChSyncQueueEvent;

typedef struct
{
    NvOsMutexHandle Mutex;
    NvOsSemaphoreHandle Semaphore;
    NvRmChSyncQueueEvent Event;
    NvU32 Read;
    NvU32 Write;
    NvU32 Buffer[NVRM_SYNC_QUEUE_SIZE];
} NvRmChSyncQueue;

/* Hardware channel state */
typedef struct NvRmChHwRec
{
    /* channel register aperture */
    NvU32 *ChannelRegs;

    /* index into RM's channel array */
    NvU32 num;

    /* lock for serializing submits */
    NvOsMutexHandle submit_mutex;

    /* pushbuffer */
    NvRmMemHandle pushbuffer;
    void *pb_mem; /* mapped pushbuffer memory */
    NvRmPhysAddr pb_addr; /* physical address of pushbuffer */

    /* byte offsets into the pushbuffer */
    NvU32 fence;
    NvU32 current;

    NvOsMutexHandle ref_mutex;
    NvU32 refcount;

    NvRmChPinBlock PinBlock;
    NvRmChSyncQueue sync_queue;
} NvRmChHw;

typedef struct NvRmSyncPointShadowRec
{
    NvU32 syncpt[NV_HOST1X_SYNCPT_NB_PTS];
    NvU32 thresh_int_mask[NV_HOST1X_SYNCPT_NB_PTS/16];
} NvRmSyncPointShadow;

typedef struct NvRmPrivChannelStateRec
{
    /* channels - allocated by NvRmPrivChannelInit() */
    NvRmChHw *ChHwState;
    NvRmChannel *Channels;
    NvU8 BaseChannel;
    NvU8 NumChannels;
    NvU8 LastChannel;

    /* sync and wait base allocation bit vectors */
    NvU32 SyncPointVector;
    NvU8 WaitBaseVector;

    NvRmSyncPointShadow SyncPointShadow;

    NvU32 *Host1xSyncBase;

    NvRmPhysAddr Host1xBasePhysical;
    NvU32 *Host1xBaseVirtual;
    NvU32 Host1xSize;

} NvRmPrivChannelState;

struct NvRmAutoClockManagerEntryRec;

typedef NvBool (*NvRmAutoClockModuleEnable)(
    NvRmDeviceHandle hRm, struct NvRmAutoClockManagerEntryRec *entry,
    NvBool enable );

/* automatic clock manager, per module state */
typedef struct NvRmAutoClockManagerEntryRec
{
    NvRmModuleID ModuleID;
    NvRmModuleID SubModules[2];
    NvU32 SyncPointID;
    NvU32 SyncPointValue;
    NvU32 TimeStampMS;
    NvU32 PowerClientID;
    NvRmChannelHandle hChannel;
    NvBool bClockEnabled;
    NvRmAutoClockModuleEnable ModuleFunc;
} NvRmAutoClockManagerEntry;

#define NVRM_AUTO_CLOCK_MODULES         1
#define NVRM_AUTO_CLOCK_MODULE_DISPLAY  (0)
#define NVRM_AUTO_CLOCK_TIMEOUT_DEFAULT (1000)
// clock in kHz
#define NVRM_AUTO_CLOCK_GRAPHICS_HOST_CLOCK_DEFAULT (83000)
#define NVRM_AUTO_CLOCK_FUDGE_MULTIPLIER            (10)

typedef struct NvRmAutoClockManagerRec
{
    // Protected by Mutex
    NvRmAutoClockManagerEntry Entries[NVRM_AUTO_CLOCK_MODULES];

    NvOsThreadHandle Thread;
    NvOsSemaphoreHandle Sem;
    NvOsSemaphoreHandle ModuleSem;
    NvOsMutexHandle Mutex;

    // These are protected by the mutex
    NvU32  LastRan;
    NvBool ModulesBusy;
    NvBool HostRegistersAreValid;
    NvBool HostVoltageEnabled;
    NvBool HostIsSuspended;

    // Host clock enable
    NvOsMutexHandle HostClockMutex;
    NvS32           HostClockRef;

    volatile NvBool Shutdown;
} NvRmAutoClockManager;

typedef struct NvRmSyncPointStatesRec
{
    NvU32 syncpt[NV_HOST1X_SYNCPT_NB_PTS];
    NvU32 thresh[NV_HOST1X_SYNCPT_NB_PTS];
    NvU32 base[NV_HOST1X_SYNCPT_NB_BASES];
    NvU32 ClassId[NV_HOST1X_CHANNELS];
} NvRmSyncPointStates;

#define HOST_CLOCK_ENABLE( hDevice ) \
        NvRmPrivHostClockEnable(hDevice)
#define HOST_CLOCK_DISABLE( hDevice ) \
        NvRmPrivHostClockDisable(hDevice)

void NvRmPrivHostClockDisable( NvRmDeviceHandle hRm );
void NvRmPrivHostClockEnable( NvRmDeviceHandle hRm );


extern NvRmPrivChannelState g_channelState;
extern NvOsMutexHandle      g_channelMutex;

/* Sync point variables */
extern NvU32  g_SyncPointWaitListCount;
extern NvBool g_SyncPointWaitListCountHoldsHostClockRef;
extern NvOsIntrMutexHandle g_SyncPointIsrMutex;

#define HOST1X_SYNC_REGW(offset, value ) \
    NV_WRITE32(( (NvUPtr)(g_channelState.Host1xSyncBase) + (offset)), (value) )

#define HOST1X_SYNC_REGR( offset) \
    NV_READ32( ((NvUPtr)(g_channelState.Host1xSyncBase) + (offset)) )

void
NvRmPrivChannelSubmitCommandBufs( NvRmChannelHandle hChannel,
    const NvRmCommandBuffer *pCommandBufs, NvU32 NumCommandBufs,
    const NvRmChannelSubmitReloc *pRelocations, const NvU32 *pRelocShifts,
    NvU32 NumRelocations,
    NvU32 SyncPointID, NvU32 SyncPointValue, NvBool NullKickoff );

/* indirect module access */
void
NvRmChModuleRegWr( NvRmDeviceHandle device, NvRmModuleID id, NvU32 num,
    const NvU32 *offsets, const NvU32 *values );

void
NvRmChModuleRegRd(NvRmDeviceHandle device, NvRmModuleID id, NvU32 num,
    const NvU32 *offsets, NvU32 *values);

/**
 * Schedule the NvRmPrivAcmThread to wake up and check whether Host1x can be
 * powered down.
 */
void NvRmPrivScheduleAcmCheck( void );

/**
 * Prepare a unit for power down.
 *
 * Limitation: this will not work if there is more than one context-switching
 * unit in a channel.
 *
 * Returns NvSuccess on success, filling out SyncPointID and SyncPointValue.
 * Wait for SyncPointValue on SyncPointID before powering down the unit.
 * After restoring the power, increment this sync point.  Do not increment the
 * shadow value; as such do not use NvRmChannelSyncPointIncr.
 *
 * Returns NvError_NotInitialized if the engine is not being used.  Nothing
 * needs to be done in this case.
 */
NvError
NvRmPrivPrepareForPowerdown( NvRmChannelHandle hChannel,
    NvRmModuleID ModuleID, NvU32 *SyncPointID, NvU32 *SyncPointValue );

/**
 * Core power down for all channel related modules including graphics host.
 * Can be used for explicit power down case for all channel related modules.
 *
 * For explicit power down, set force to NV_TRUE.
 * pSleeptime for internal Acm Thread.  Pass NULL for explciit power down.
 */
void NvRmChPrivPowerdown( NvRmDeviceHandle hRm, NvBool force,
                        NvU32 *pSleeptime );


typedef enum
{
    /**
     * Signal a semaphore.
     * 'data' points to a NvOsSemaphoreHandle
     * NvOsSempahoreWait() must be used to block until the semaphore is signaled
     */
    NvRmWaitListAction_Wait = 1,

    /**
     * Update a channel's sync queue
     * 'data' points to a NvRmChannelHandle
     * NvRmPrivUpdateSyncQueue() is called from a worker thread
     * TODO: we could maybe make this a more general callback, with context arg
     */
    NvRmWaitListAction_Submit,

    NvRmWaitListAction_Force32 = 0x7fffffffUL
} NvRmWaitListAction;

/**
 * Schedule an action to be taken when a sync point reaches the given threshold.
 *
 * @param hDevice RM instance
 * @param id the sync point
 * @param thresh the threshold
 * @param action the action to take
 * @param data a pointer to extra data depending on action, see above
 *
 * This is a non-blocking api.
 */
NvError
NvRmPrivWaitListSchedule(NvRmDeviceHandle hDevice, NvU32 id, NvU32 thresh,
                         NvRmWaitListAction action, void* data);

/**
 * Schedule a semaphore to be signaled when the sync point reaches the given
 * threshold.
 */
static NV_INLINE NvError
NvRmPrivSyncPointSchedule( NvRmDeviceHandle hDevice, NvU32 id, NvU32 thresh,
    NvOsSemaphoreHandle sem )
{
    return NvRmPrivWaitListSchedule(hDevice, id, thresh,
                                    NvRmWaitListAction_Wait, &sem);
}

/**
 * Schedule a callback to NvRmPrivUpdateSyncQueue for the given channel when
 * the sync point reaches the given threshold.
 */
static NV_INLINE NvError
NvRmPrivSubmitSchedule( NvRmDeviceHandle hDevice, NvU32 id, NvU32 thresh,
    NvRmChannelHandle hChannel )
{
    return NvRmPrivWaitListSchedule(hDevice, id, thresh,
                                    NvRmWaitListAction_Submit, &hChannel);
}

/**
 * Called when end-of-submit sync point is reached for the given channel.
 * Processes finished sync queue entries, unpins/unrefs hMems, bumps push buffer
 */
void NvRmPrivUpdateSyncQueue(NvRmChannelHandle hChannel);


void
NvRmChPrivSaveSyncPoints( NvRmDeviceHandle hDevice );
void
NvRmChPrivRestoreSyncPoints( NvRmDeviceHandle hDevice );
void
NvRmChPrivSaveMlocks( NvRmDeviceHandle hDevice );
void
NvRmChPrivIncrSyncpt(NvU32 SyncPointID);

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif // INCLUDED_AP20_CHANNEL_H
