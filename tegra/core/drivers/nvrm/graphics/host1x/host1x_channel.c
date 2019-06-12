/*
 * Copyright (c) 2007 - 2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvos.h"
#include "nvutil.h"
#include "nvrm_channel.h"
#include "nvrm_memmgr.h"
#include "nvrm_drf.h"
#include "nvassert.h"
#include "nverror.h"
#include "nvrm_rmctrace.h"
#include "arhost1x.h"
#include "arhost1x_channel.h"
#include "arhost1x_sync.h"
#include "host1x_module_ids.h"
#include "project_arhost1x_sw_defs.h"
#include "host1x_channel.h"
#include "host1x_hwcontext.h"
#include "nvrm_graphics_private.h"
#include "nvrm_channel_priv.h"

#define CHANNEL_DUMP 0
#define CHANNEL_HISTOGRAM 0
#define NVRM_ACM_STATE_DEBUG 0
#define NVRM_ACM_VERBOSE_DEBUG 0

static void NvRmPrivResetSyncpt( NvRmDeviceHandle hDevice, NvU32 id );
static void NvRmPrivResetModuleSyncPoints( NvRmDeviceHandle hDevice );
static NvError NvRmPrivIncrSyncPointShadow( NvRmDeviceHandle hDevice, NvU32 id,
    NvU32 incr, NvU32 *oldValue, NvU32 *value );
static void NvRmPrivChannelFlush( NvRmChannelHandle hChannel );
static void NvRmPrivChannelReset( NvRmChannelHandle hChannel );
static NvError NvRmPrivAllocateChannelResources(
    NvRmChannelHandle hChannel );
static NvBool NvRmChPrivSyncPointSignalSemaphore( NvRmDeviceHandle hDevice,
    NvU32 SyncPointID, NvU32 Threshold, NvOsSemaphoreHandle hSema);
static NvU32 NvRmChPrivSyncPointRead(NvRmDeviceHandle hDevice,
    NvU32 SyncPointID);
static void NvRmChPrivSyncPointIncr(NvRmDeviceHandle hDevice,
    NvU32 SyncPointID);
static NvError NvRmPrivAcmInit( NvRmDeviceHandle hRm );
static void NvRmPrivAcmDeinit( void );
static void NvRmPrivAcmThread( void *args );
NvError NvRmPowerSuspend( NvRmDeviceHandle hDevice );
NvError NvRmPowerResume( NvRmDeviceHandle hDevice );

/* submit & sync queue stuff */
static void IncrRefMult(NvRmMemHandle* handles, NvU32 count);
static void DecrRefMult(NvRmMemHandle* handles, NvU32 count);

static void SyncQueueReset(NvRmChSyncQueue *hw);
static NvU32 SyncQueueGetFreeSpace(NvRmChSyncQueue *hw);
static void SyncQueueAdd(NvRmChSyncQueue *hw,
    NvU32 SyncPointID, NvU32 SyncPointValue,
    NvU32 NumGathers, NvRmMemHandle* Handles, NvU32 NumHandles);
static NvU32* SyncQueueHead(NvRmChSyncQueue *hw);
static void SyncQueueDequeue(NvRmChSyncQueue *hw);

static void PinBlockInit(NvRmChPinBlock* pb, NvU32* Mem, NvU32 Capacity);
static NvError PinBlockEnsureCapacity(NvRmChPinBlock* pb, NvU32 Needed);
static void PinBlockProcessRelocations(NvRmChPinBlock* pb,
    const NvRmChannelSubmitReloc* reloc, NvU32 count);
static void PinBlockAddCommandBuffers(NvRmChPinBlock* pb,
    const NvRmCommandBuffer* pCommandBufs, NvU32 NumCommandBufs);

static void PatchRelocations(const NvRmChannelSubmitReloc *pRelocations,
    const NvU32 *pRelocShifts, NvU32 NumRelocations,
    NvU32* PinBlockPhys, NvU32* PinBlockIndex);
static void KickOffCommandBuffers(NvRmChannelHandle hChannel,
    const NvRmCommandBuffer *pCommandBufs, NvU32 NumCommandBufs,
    NvU32* CmdBufPhys, NvBool NullKickoff);
static void EnqueueCleanupWork(NvRmChannelHandle hChannel,
    NvU32 SyncPointID, NvU32 SyncPointValue,
    NvU32 NumGathers, NvRmMemHandle* Handles, NvU32 NumHandles);

/* automatic clock manager stuff */
static NvRmAutoClockManager s_Acm;
static NvU32                s_AutoClockTimeout;
/* host1x suspend/resume stuff */
#define NVRM_CHANNEL_DISP   0
#define NVRM_CHANNEL_DSI    6
static NvError NvRmChPrivEnableHost( NvRmDeviceHandle hDevice );
static void    NvRmChPrivDisableHost( NvRmDeviceHandle hDevice );
static void    NvRmChPrivRestoreMlocks( NvRmDeviceHandle hDevice );

static NvRmSyncPointStates  s_SyncPointStates;
static NvU32 s_MlockState[NV_HOST1X_NB_MLOCKS];

/* Global channel state, protected by g_channelMutex */
NvRmPrivChannelState g_channelState;
NvOsMutexHandle      g_channelMutex;

/*
 * These are used in asserts
 */
#define CHANNEL_IS_INITIALIZED() ((g_channelState.Channels != NULL) && \
                                  (s_Acm.Mutex != NULL) && \
                                  (s_Acm.HostClockMutex != NULL) && \
                                  (s_Acm.Thread != NULL) && \
                                  (s_Acm.Sem    != NULL))
#define HOST_IS_POWERED()        (CHANNEL_IS_INITIALIZED() && \
                                  s_Acm.HostRegistersAreValid)
#define HOST_IS_CLOCKED()        (HOST_IS_POWERED() && \
                                  s_Acm.HostClockRef != 0)

/* clock machinations */
static NvU32 g_HostPowerClientID;

#if     NVRM_ACM_STATE_DEBUG
// Print when ACM (Automatic Clock Management) state changes occur.
#define ACM_PRINTF(m)    NvOsDebugPrintf m
#define MODSTR_X(m,x)       ((m)==(NvRmModuleID_##x))?#x:
#define MODSTR1(m) ( \
        MODSTR_X(m,Display) \
        MODSTR_X(m,GraphicsHost) \
        "???")
#define MODSTR(m) MODSTR1(NVRM_MODULE_ID_MODULE(m))
#define MODPNT(m) ((int)(m)),MODSTR(m)
#if NVRM_ACM_VERBOSE_DEBUG
#define ACMV_PRINTF(m)    NvOsDebugPrintf m
#else
#define ACMV_PRINTF(m)
#endif
#else
#define ACM_PRINTF(m)
#define ACMV_PRINTF(m)
#endif

/* Only one thread can read write at a time */
static NvOsMutexHandle g_ModuleRegRdWrLock;

/* Sync point variables */
NvU32  g_SyncPointWaitListCount = 0;
NvBool g_SyncPointWaitListCountHoldsHostClockRef = NV_FALSE;
NvOsIntrMutexHandle g_SyncPointIsrMutex;

static NvRmRmcFile *s_rmc;

#define NVRM_T30_QT_WAR 1

static NvError NvRmPrivMutexAlloc(NvOsMutexHandle *m)
{
    NvError err;
    NvOsMutexHandle m0;
    err = NvOsMutexCreate(&m0);
    if (!err)
    {
        if (NvOsAtomicCompareExchange32(
                    (NvS32 *)m, 0, (NvS32)m0) != 0)
        {
            NvOsMutexDestroy(m0);
        }
    }
    return *m ? NvSuccess : err;
}



// Note: Assumes s_Acm.Mutex is locked!!
static NV_INLINE NvError NvRmChPrivHostVoltageEnable( NvRmDeviceHandle hDevice )
{
    NvError err = NvSuccess;

    if (!s_Acm.HostVoltageEnabled)
    {
        // This can only fail the first time it is called (allocates memory)
        err = NvRmPowerVoltageControl( hDevice,
                NvRmModuleID_GraphicsHost,
                g_HostPowerClientID, NvRmVoltsUnspecified,
                NvRmVoltsUnspecified,
                NULL, 0, NULL );

        ACM_PRINTF(("===== HOST VOLTAGE ON\n"));
        if (!err)
            s_Acm.HostVoltageEnabled = NV_TRUE;
    }

    return err;
}

// Note: Assumes s_Acm.Mutex is locked!!
static NV_INLINE void NvRmChPrivHostVoltageDisable( NvRmDeviceHandle hDevice )
{
    if (s_Acm.HostVoltageEnabled)
    {
        ACM_PRINTF(("===== HOST VOLTAGE OFF\n"));
        s_Acm.HostVoltageEnabled = NV_FALSE;
        // This cannot fail (it only allocates memory the first time it was
        // called, which was when we turned the power on).
        NV_ASSERT_SUCCESS(NvRmPowerVoltageControl(
                hDevice, NvRmModuleID_GraphicsHost,
                g_HostPowerClientID, NvRmVoltsOff, NvRmVoltsOff,
                NULL, 0, NULL ));
    }
}

//
// This is called when modules under the host become busy.  Call power
// management stuff from here.
//
// Note: Assumes s_Acm.Mutex is locked!!
static NV_INLINE void NvRmChPrivHostIsBusy( NvRmDeviceHandle hDevice )
{
    NV_ASSERT(!s_Acm.ModulesBusy);
    s_Acm.ModulesBusy = NV_TRUE;
    HOST_CLOCK_ENABLE( hDevice );

    // This is a hack.  It works because NvRmPowerVoltageControl() does not
    // actually affect voltage for the host, but it does affect the reference
    // counting scheme for WinCE power management.
    //
    // In Linux this call is not needed.
    NV_ASSERT_SUCCESS( NvRmChPrivHostVoltageEnable(hDevice) );
}

//
// This is called when all modules under the host are no longer busy.  Call
// power management stuff from here.
//
// Note: Assumes s_Acm.Mutex is locked!!
static NV_INLINE void NvRmChPrivHostIsNotBusy( NvRmDeviceHandle hDevice )
{
    NV_ASSERT(s_Acm.ModulesBusy);
    s_Acm.ModulesBusy = NV_FALSE;
    HOST_CLOCK_DISABLE( hDevice );

    // This is a hack.  It works because NvRmPowerVoltageControl() does not
    // actually affect voltage for the host, but it does affect the reference
    // counting scheme for WinCE power management.
    //
    // In Linux this call is not needed.
    NvRmChPrivHostVoltageDisable(hDevice);
}


NvError
NvRmPrivChannelInit( NvRmDeviceHandle rm )
{
    NvError err;
    NvU32 i;
    NvU32 size;
    NvU32 num = (NV_HOST1X_CHANNELS - 1);
    NvU32 base = 0;
    NvRmFreqKHz TargetKHz = 0;

    /* allocate an array of channels. Use all the channels leaving one for
     * AVP
     */
    g_channelState.NumChannels = (NvU8)num;
    g_channelState.BaseChannel = (NvU8)base;
    g_channelState.LastChannel = (NvU8)base;
    size = (sizeof(NvRmChannel) + sizeof(NvRmChHw)) * num;
    g_channelState.Channels = NvOsAlloc( size );
    g_channelState.ChHwState = (NvRmChHw*)(g_channelState.Channels + num);

    if( !g_channelState.Channels || !g_channelState.ChHwState )
    {
        goto fail;
    }

    NvOsMemset( g_channelState.Channels, 0, sizeof(NvRmChannel) * num );
    NvOsMemset( g_channelState.ChHwState, 0, sizeof(NvRmChHw) * num );

    // create the indirect read/write mutex
    err = NvOsMutexCreate(&g_ModuleRegRdWrLock);
    if( err != NvSuccess )
    {
        goto fail;
    }

    NvRmModuleGetBaseAddress( rm,
        NVRM_MODULE_ID(NvRmModuleID_GraphicsHost, 0),
        &(g_channelState.Host1xBasePhysical), &(g_channelState.Host1xSize));

    err = NvRmPhysicalMemMap(g_channelState.Host1xBasePhysical,
            g_channelState.Host1xSize, NVOS_MEM_READ_WRITE,
            NvOsMemAttribute_Uncached,
            (void **)&(g_channelState.Host1xBaseVirtual));
    if( err != NvSuccess )
    {
        goto fail;
    }

    g_channelState.Host1xSyncBase =
        (NvU32 *)((NvUPtr)(g_channelState.Host1xBaseVirtual) +
        NV_HOST1X_CHANNEL0_BASE + HOST1X_CHANNEL_SYNC_REG_BASE);

    /* setup sync point vector (reserved bits) */
    g_channelState.SyncPointVector = 0;
    g_channelState.SyncPointVector |= NVRM_SYNCPOINTS_RESERVED;

    for( i = base; i < num; i++ )
    {
        NvRmChannel *ch = &g_channelState.Channels[ i ];
        NvRmChHw *hw = &g_channelState.ChHwState[ i ];

        ch->hDevice = rm;
        ch->HwState = hw;
        hw->num = i;

        /* create the mutex for sync queue updating */
        err = NvOsMutexCreate( &hw->sync_queue.Mutex );
        if( err != NvSuccess )
        {
            goto fail;
        }
        SyncQueueReset(&hw->sync_queue);

        PinBlockInit(&hw->PinBlock, NULL, 0);
        hw->ChannelRegs = (NvU32 *)((NvUPtr)(g_channelState.Host1xBaseVirtual)
            + NV_HOST1X_CHANNEL0_BASE +
            (i*NV_HOST1X_CHANNEL_MAP_SIZE_BYTES));

        /* always create the refount mutex to keep synchronization tractable */
        err = NvOsMutexCreate(&hw->ref_mutex);
        if( err != NvSuccess )
        {
            goto fail;
        }

        hw->refcount = 0;
    }

    /* Global channel mutex */
    err = NvOsMutexCreate(&g_channelMutex);
    if( err != NvSuccess )
    {
        goto fail;
    }

    err = NvRmPrivAcmInit(rm);
    if (err)
        goto fail;


    g_HostPowerClientID = NVRM_POWER_CLIENT_TAG('H','S','T','1');
    err = NvRmPowerRegister( rm, 0, &g_HostPowerClientID );
    if (err)
    {
        NV_ASSERT( !"NvRmPowerRegister on GraphicsHost fail" );
        goto fail;
    }

    NvOsMutexLock( s_Acm.Mutex );
    s_Acm.HostIsSuspended = NV_FALSE;
    err = NvRmChPrivEnableHost( rm );
    NvOsMutexUnlock( s_Acm.Mutex );

    if (err)
    {
        NV_ASSERT( !"NvRmChPrivEnableHost on GraphicsHost fail" );
        goto fail;
    }

    HOST_CLOCK_ENABLE( rm );

    /* Set initial host1x clock to max and update microsecond counter.
     * Read back actual frequency to decide Acm timeout value.
     * If host1x clock is < default (83MHz), it's most likely running on FPGA.
     * Then, a (fudge * scaling) be applied to default timeout value.
     * Scaling = round-up of (default_host1x_freq / actual_freq)
     */
    TargetKHz = NvRmFreqMaximum;    // maximum at RM discretion
    err = NvRmPowerModuleClockConfig(
            rm, NvRmModuleID_GraphicsHost, 0, NvRmFreqUnspecified,
            NvRmFreqUnspecified, &TargetKHz, 1, &TargetKHz, 0);
    HOST1X_SYNC_REGW(HOST1X_SYNC_USEC_CLK_0, (TargetKHz + 1000 - 1)/1000 );
    if( err != NvSuccess )
    {
        HOST_CLOCK_DISABLE( rm );
        goto fail;
    }

    if ( TargetKHz >= NVRM_AUTO_CLOCK_GRAPHICS_HOST_CLOCK_DEFAULT )
    {
        s_AutoClockTimeout = NVRM_AUTO_CLOCK_TIMEOUT_DEFAULT;
    }
    else
    {
        s_AutoClockTimeout = NVRM_AUTO_CLOCK_FUDGE_MULTIPLIER *
            NVRM_AUTO_CLOCK_TIMEOUT_DEFAULT *
            ((NVRM_AUTO_CLOCK_GRAPHICS_HOST_CLOCK_DEFAULT + (TargetKHz - 1)) /
                TargetKHz);
    }

    /* clear the pre-allocated sync points and wait-base registers */
    NvRmPrivResetModuleSyncPoints( rm );

    NvRmGetRmcFile( rm, &s_rmc );

    HOST_CLOCK_DISABLE( rm );

    return NvSuccess;

fail:
    NvRmPrivAcmDeinit();
    NvOsFree( g_channelState.Channels );
    g_channelState.Channels = 0;
    g_channelState.ChHwState = 0;

    // FIXME: cleanup
    return NvError_RmChannelInitFailure;
}

void
NvRmPrivChannelDeinit( NvRmDeviceHandle rm )
{
    NvU32 i;
    NvRmChHw *hw;

    if( !g_channelState.Channels )
        return;

    if (!s_Acm.HostIsSuspended)
        (void)NvRmPowerSuspend( rm );

    NV_ASSERT(g_channelState.ChHwState);

    NvRmPrivAcmDeinit();

    /* stop the channels; deallocate resources */
    for( i = g_channelState.BaseChannel; i < g_channelState.NumChannels; i++ )
    {
        NV_ASSERT( &g_channelState.Channels[i] );
        NV_ASSERT( (&g_channelState.Channels[i])->HwState );

        NvRmChannelClose( &g_channelState.Channels[i] );

        hw = (&g_channelState.Channels[i])->HwState;

        NvOsMutexDestroy(hw->sync_queue.Mutex);

        /* this is the only place it's safe to destroy the channel's
         * ref_mutex
         */
        NvOsMutexDestroy( hw->ref_mutex );
    }

    NvOsFree( g_channelState.Channels );
    g_channelState.Channels = 0;
    g_channelState.ChHwState = 0;

    NvRmPhysicalMemUnmap((void *)g_channelState.Host1xBaseVirtual,
            g_channelState.Host1xSize);
    g_channelState.Host1xBaseVirtual = NULL;
    g_channelState.Host1xSize = 0;
    NvOsMutexDestroy( g_channelMutex );
    NvOsMutexDestroy( g_ModuleRegRdWrLock );
    g_channelMutex = NULL;
}

static NvError
NvRmPrivAllocateChannelResources( NvRmChannelHandle hChannel )
{
#if NV_DEF_RMC_TRACE
    NvBool RmcEnabled;
#endif
    NvError err;
    NvU32 reg;
    NvU32 restart;
    NvRmChHw *hw;

    NV_ASSERT( hChannel );
    NV_ASSERT( hChannel->hDevice );

    hw = hChannel->HwState;
    NV_ASSERT( hw );

    NvOsMutexLock( hw->ref_mutex );

    if( hw->refcount > 0 )
    {
        hw->refcount++;
        NvOsMutexUnlock( hw->ref_mutex );
        return NvSuccess;
    }

    /* create the mutex for submits */
    err = NvOsMutexCreate(&hw->submit_mutex);
    if( err != NvSuccess )
    {
        goto fail;
    }

    /* create the semaphore for sync point signalling */
    err = NvOsSemaphoreCreate( &hChannel->Semaphore, 0 );
    if( err != NvSuccess )
    {
        goto fail;
    }

    /* Allocate the PinBlock for managing atomic pinning of submitted handles */
    err = PinBlockEnsureCapacity(&hw->PinBlock, NVRM_CHANNEL_SUBMIT_MAX_HANDLES);
    if( err != NvSuccess )
    {
        goto fail;
    }

    // The sync queue mutex is created at channel init time

    /* create the semaphore for sync queue filling */
    err = NvOsSemaphoreCreate( &hw->sync_queue.Semaphore, 0 );
    if( err != NvSuccess )
    {
        goto fail;
    }

    /* get memory for the channel's pushbuffer - stash a restart opcode
     * at the end of the pushbuffer - don't let it get over written
     * (hide the actual size from other channel functions)
     */
    err = NvRmMemHandleAlloc( hChannel->hDevice, NULL, 0, 32,
        NvOsMemAttribute_Uncached,
        NVRM_PUSHBUFFER_SIZE + 4, NVRM_MEM_TAG_RM_MISC, 1, &hw->pushbuffer);
    if( err != NvSuccess )
    {
        goto fail;
    }

    /* map the memory - will have to use indirect writes if this fails */
    err = NvRmMemMap( hw->pushbuffer, 0, NVRM_PUSHBUFFER_SIZE,
        NVOS_MEM_READ_WRITE, &hw->pb_mem );
    if( err != NvSuccess )
    {
       hw->pb_mem = 0;
    }

    /* leave one hole in the gather fifo (easier) */
    hw->fence = NVRM_PUSHBUFFER_SIZE - 8;
    hw->current = 0;

    HOST_CLOCK_ENABLE( hChannel->hDevice );
    /* disable DMA */
    reg = NV_DRF_DEF( HOST1X_CHANNEL, DMACTRL, DMASTOP, STOP );
    CHANNEL_REGW( hw, DMACTRL, reg );

    /* pin pushbuffer and get physical address */
    hw->pb_addr = NvRmMemPin(hw->pushbuffer);

    /* put the restart at the end of pushbuffer memory */
    restart = NVOPCODE_RESTART( hw->pb_addr >> 4 );
#if NV_DEF_RMC_TRACE
    /* FIXME cannot call RMC APIs from channel code. Need to fix this.*/
    RmcEnabled = NVRM_RMC_IS_ENABLED(s_rmc);
    NVRM_RMC_ENABLE(s_rmc, NV_FALSE);
#endif
    NvRmMemWr32(hw->pushbuffer, NVRM_PUSHBUFFER_SIZE, restart);
#if NV_DEF_RMC_TRACE
    NVRM_RMC_ENABLE(s_rmc, RmcEnabled);
#endif

    /* set base, put, end pointer (all of memory) */
    CHANNEL_REGW( hw, DMASTART, 0 );
    CHANNEL_REGW( hw, DMAPUT, hw->pb_addr );
    CHANNEL_REGW( hw, DMAEND, 0xFFFFFFFF );

    /* reset GET */
    reg = NV_DRF_DEF( HOST1X_CHANNEL, DMACTRL, DMASTOP, STOP )
        | NV_DRF_DEF( HOST1X_CHANNEL, DMACTRL, DMAGETRST, ENABLE )
        | NV_DRF_DEF( HOST1X_CHANNEL, DMACTRL, DMAINITGET, ENABLE );
    CHANNEL_REGW( hw, DMACTRL, reg );

    /* start the command DMA */
    reg = NV_DRF_DEF( HOST1X_CHANNEL, DMACTRL, DMASTOP, RUN );
    CHANNEL_REGW( hw, DMACTRL, reg );

    HOST_CLOCK_DISABLE( hChannel->hDevice );
    hw->refcount = 1;

    err = NvSuccess;
    goto clean;

fail:
    err = NvError_RmChannelInitFailure;

clean:
    NvOsMutexUnlock( hw->ref_mutex );
    return err;
}

static void
NvRmPrivChannelReset( NvRmChannelHandle hChannel )
{
    /* synchronization should be handled by the caller */

    NvU32 reg;
    NvRmChHw *hw;

    hw = hChannel->HwState;

    /* stop the command DMA */
    reg = NV_DRF_DEF( HOST1X_CHANNEL, DMACTRL, DMASTOP, STOP );
    CHANNEL_REGW( hw, DMACTRL, reg );

    /* reset the gather fifo and sync queue */
    hw->fence = NVRM_PUSHBUFFER_SIZE - 8;
    hw->current = 0;
    SyncQueueReset(&hw->sync_queue);

    /* reset put */
    CHANNEL_REGW( hw, DMAPUT, hw->pb_addr );

    /* reset GET */
    reg = NV_DRF_DEF( HOST1X_CHANNEL, DMACTRL, DMASTOP, STOP )
        | NV_DRF_DEF( HOST1X_CHANNEL, DMACTRL, DMAGETRST, ENABLE )
        | NV_DRF_DEF( HOST1X_CHANNEL, DMACTRL, DMAINITGET, ENABLE );
    CHANNEL_REGW( hw, DMACTRL, reg );

    /* start the command DMA */
    reg = NV_DRF_DEF( HOST1X_CHANNEL, DMACTRL, DMASTOP, RUN );
    CHANNEL_REGW( hw, DMACTRL, reg );
}

NvError
NvRmChannelOpen(
    NvRmDeviceHandle hDevice,
    NvRmChannelHandle *phChannel,
    NvU32 NumModules,
    const NvRmModuleID *pModuleIDs)
{
    NvU32 i;
    NvU32 mod;
    NvU32  channel = (NvU32)-1;
    NvBool useDisplay = NV_FALSE;
    NvBool useDsi = NV_FALSE;
    NvError err;

    NV_ASSERT( hDevice );
    NV_ASSERT( phChannel );

    NV_ASSERT(g_channelState.Channels);

    NvOsMutexLock( g_channelMutex );

    for (i = 0; i < NumModules; i++)
    {
        mod = NVRM_MODULE_ID_MODULE( pModuleIDs[i] );

        if ( mod == NvRmModuleID_Display)
            useDisplay = NV_TRUE;
        else if( mod == NvRmModuleID_Dsi )
        {
            useDsi = NV_TRUE;
        }
        else
        {
            NV_ASSERT(!"Not sure what to do with this module");
        }
    }

    if (useDisplay)
    {
        channel = NVRM_CHANNEL_DISP;
    }
    else if (useDsi)
    {
        channel = NVRM_CHANNEL_DSI;
    }
    else
    {
        NV_ASSERT(!"No modules were passed into ChannelOpen");
    }

    *phChannel = &g_channelState.Channels[channel];

    err = NvRmPrivAllocateChannelResources( *phChannel );
    if( err != NvSuccess )
    {
        goto fail;
    }

    NvOsMutexUnlock( g_channelMutex );
    return NvSuccess;

fail:
    NvOsMutexUnlock( g_channelMutex );
    return NvError_RmChannelInitFailure;
}

void
NvRmChannelClose(NvRmChannelHandle hChannel)
{
    NvU32 reg;
    NvRmChHw *hw;

    if( !hChannel )
    {
        return;
    }

    hw = hChannel->HwState;
    NV_ASSERT( hw );

    NvOsMutexLock( hw->ref_mutex );

    if ( hw->refcount )
    {
        hw->refcount--;

        if( !hw->refcount )
        {
            NvRmPrivChannelFlush( hChannel );

            /* disable DMA */
            HOST_CLOCK_ENABLE( hChannel->hDevice );
            reg = NV_DRF_DEF( HOST1X_CHANNEL, DMACTRL, DMASTOP, STOP );
            CHANNEL_REGW( hw, DMACTRL, reg );
            HOST_CLOCK_DISABLE( hChannel->hDevice );

            NvOsMutexDestroy( hw->submit_mutex );
            NvOsSemaphoreDestroy( hChannel->Semaphore );

            NvRmMemUnmap( hw->pushbuffer, hw->pb_mem, NVRM_PUSHBUFFER_SIZE );
            NvRmMemUnpin( hw->pushbuffer );

            NvRmMemHandleFree( hw->pushbuffer );
            hw->pushbuffer = 0;

            PinBlockEnsureCapacity(&hw->PinBlock, 0);

            NvOsSemaphoreDestroy(hw->sync_queue.Semaphore);
        }
    }

    /* do NOT destroy the ref_mutex */
    NvOsMutexUnlock( hw->ref_mutex );
}

NvError
NvRmChannelAlloc( NvRmDeviceHandle hDevice, NvRmChannelHandle *phChannel )
{
#if NV_DEBUG
    NvError err;
    NvU32 i;
    NvBool found = NV_FALSE;

    NvOsMutexLock( g_channelMutex );

    /* don't touch first channel */
    for( i = g_channelState.BaseChannel + 1;
         i < g_channelState.NumChannels; i++ )
    {
        NvRmChannel *ch = &g_channelState.Channels[ i ];
        NvRmChHw *hw = ch->HwState;

        NvOsMutexLock( hw->ref_mutex );

        if( hw->refcount == 0 )
        {
            /* need to allocate the channel resources */
            err = NvRmPrivAllocateChannelResources( ch );
            if( err != NvSuccess )
            {
                NvOsMutexUnlock( hw->ref_mutex );
                goto clean;
            }

            *phChannel = ch;
            found = NV_TRUE;
        }

        NvOsMutexUnlock( hw->ref_mutex );

        if( found )
        {
            break;
        }
    }

clean:
    NvOsMutexUnlock( g_channelMutex );

    /* return a strange error code just for fun */
    return ( found ) ? NvSuccess : NvError_InvalidState;
#else
    return NvError_NotSupported;
#endif
}

#if CHANNEL_DUMP
static void
NvRmPrivDumpCommandBuffers( NvU32 num, const NvRmCommandBuffer *pCommandBufs,
    NvU32 NumCommandBufs )
{
    static NvOsFileHandle s_file[NV_HOST1X_CHANNELS] = { 0 };
    NvError err;
    NvU32 i, j, k, words;
    NvRmCommandBuffer const *cmd;

    if( !s_file[num] )
    {
        char buff[NVOS_PATH_MAX];
        NvOsSnprintf( buff, sizeof(buff), "command_buffer_dump_%d.txt", num );
        err = NvOsFopen( buff, NVOS_OPEN_WRITE | NVOS_OPEN_CREATE,
            &s_file[num] );
        if( err != NvSuccess )
        {
            s_file[num] = 0;
            return;
        }
    }

    if( CHANNEL_HISTOGRAM )
    {
        /* just print a size per line of the file */
        for( i = 0; i < NumCommandBufs; i++ )
        {
            cmd = &pCommandBufs[i];
            NvOsFprintf( s_file[num], "%d\n", cmd->Words );
            NvOsFflush( s_file[num] );
        }
    }
    else
    {
        for( i = 0; i < NumCommandBufs; i++ )
        {
            NvU32 PhysAddr;

            cmd = &pCommandBufs[i];
            PhysAddr = NvRmMemPin(cmd->hMem);
            NvOsFprintf( s_file[num],
                "buffer[%d]: size (words): %d addr: %x\n", i,
                cmd->Words, PhysAddr);
            NvRmMemUnpin(cmd->hMem);

            for( words = 0, j = 0; words < cmd->Words; j++ )
            {
                NvOsFprintf( s_file[num], "\t" );
                for( k = 0; k < 10 && words < cmd->Words; k++, words++ )
                {
                    NvU32 data;
                    data = NvRmMemRd32( cmd->hMem, cmd->Offset +
                        ((j * 10) + k ) * 4);
                    NvOsFprintf( s_file[num], "%.8x ", data );
                }
                NvOsFprintf( s_file[num], "\n" );
            }

            NvOsFflush( s_file[num] );
            NvOsFprintf( s_file[num], "\n" );
        }
    }
}
#endif

static NvBool
NvRmPrivAcmModuleDisable( NvRmDeviceHandle hRm,
    NvRmAutoClockManagerEntry *entry )
{
    NvError e;
    NvU32 i;

    if( NVRM_MODULE_ID_MODULE(entry->ModuleID) == NvRmModuleID_Display )
    {
        entry->bClockEnabled = NV_FALSE;
        return NV_TRUE;
    }

    if( entry->ModuleFunc )
    {
        entry->ModuleFunc( hRm, entry, NV_FALSE );
    }

    NV_CHECK_ERROR_CLEANUP(
        NvRmPowerModuleClockControl( hRm, entry->ModuleID,
            entry->PowerClientID, NV_FALSE )
    );

    for( i = 0; entry->SubModules[i] != NvRmModuleID_Invalid; i++ )
    {
        // BUG 622219 - reset the EPP block to prevent power drain
        if( entry->SubModules[i]==NvRmModuleID_Epp )
        {
            NvRmModuleReset(hRm,NvRmModuleID_Epp);
        }

        NV_CHECK_ERROR_CLEANUP(
            NvRmPowerModuleClockControl( hRm,
                entry->SubModules[i], entry->PowerClientID, NV_FALSE )
        );
        NV_CHECK_ERROR_CLEANUP(
            NvRmPowerVoltageControl( hRm, entry->SubModules[i],
                entry->PowerClientID, NvRmVoltsOff, NvRmVoltsOff,
                NULL, 0, NULL )
        );
    }

    ACM_PRINTF(("===== Mod %08x %s pwrOFF\n",MODPNT(entry->ModuleID)));
    NV_CHECK_ERROR_CLEANUP(
        NvRmPowerVoltageControl( hRm, entry->ModuleID,
            entry->PowerClientID, NvRmVoltsOff, NvRmVoltsOff,
            NULL, 0, NULL )
    );

    entry->bClockEnabled = NV_FALSE;

    return NV_TRUE;

fail:
    return NV_FALSE;
}

/* Check to see if modules can be turned off
 *
 * NOTE: assumes s_Acm.Mutex is locked
 */
void
NvRmChPrivPowerdown( NvRmDeviceHandle hRm, NvBool force, NvU32 *pSleeptime )
{
    NvU32 currenttime;
    NvU32 i;

    if ( pSleeptime )
        *pSleeptime = NV_WAIT_INFINITE;

    currenttime = NvOsGetTimeMS();

    if (s_Acm.ModulesBusy)
    {
        NvBool isNoLongerBusy = NV_TRUE;

        /* check for busy modules, sync point/value, time stamp. */
        for( i = 0; i < NVRM_AUTO_CLOCK_MODULES; i++ )
        {
            NvRmAutoClockManagerEntry *entry = &s_Acm.Entries[i];
            if (!entry->bClockEnabled)
                continue;

            if ((currenttime - entry->TimeStampMS) < s_AutoClockTimeout &&
                !force)
            {
                isNoLongerBusy = NV_FALSE;
                continue;
            }

            for (;;)
            {
                NvU32 val = NvRmChPrivSyncPointRead( hRm, entry->SyncPointID );
                if( (NvS32)(val - entry->SyncPointValue) >= 0 )
                {
                    /* the clock may be disabled */
                    if( NvRmPrivAcmModuleDisable( hRm, entry ) )
                        break;
                }
                if (!force)
                {
                    isNoLongerBusy = NV_FALSE;
                    break;
                }
            }
        }

        if (isNoLongerBusy)
        {
            NvRmChPrivHostIsNotBusy( hRm );
        }
        else
        {
            if (pSleeptime)
                *pSleeptime = s_AutoClockTimeout;
        }
    }

    if (g_SyncPointWaitListCountHoldsHostClockRef)
    {
        NvBool disableHostClockForWaitList = NV_FALSE;

        NvOsIntrMutexLock( g_SyncPointIsrMutex );
        if (g_SyncPointWaitListCount == 0 &&
            g_SyncPointWaitListCountHoldsHostClockRef)
        {
            g_SyncPointWaitListCountHoldsHostClockRef = NV_FALSE;
            disableHostClockForWaitList = NV_TRUE;
        }
        NvOsIntrMutexUnlock( g_SyncPointIsrMutex );

        if (disableHostClockForWaitList)
        {
            // This balances the HOST_CLOCK_ENABLE() in
            // NvRmPrivSyncPointSchedule()
            HOST_CLOCK_DISABLE( hRm );
        }
    }

    s_Acm.LastRan = currenttime;
}

static void
NvRmPrivAcmDeinit( void )
{
    if( s_Acm.Thread )
    {
        s_Acm.Shutdown = NV_TRUE;
        NvOsSemaphoreSignal( s_Acm.Sem );
        NvOsThreadJoin( s_Acm.Thread );
    }

    NvOsSemaphoreDestroy( s_Acm.Sem );
    NvOsSemaphoreDestroy( s_Acm.ModuleSem );
    NvOsMutexDestroy( s_Acm.Mutex );

    NvOsMemset( &s_Acm, 0, sizeof(s_Acm) );
}

static NvError
NvRmPrivAcmInit( NvRmDeviceHandle hRm )
{
    NvError err;

    /* atomically create the ACM mutex, if necessary */
    if( !s_Acm.Mutex )
    {
        NvOsMutexHandle m;
        err = NvOsMutexCreate( &m );
        if (err)
            return err;

        if( NvOsAtomicCompareExchange32(
                (NvS32 *)&s_Acm.Mutex, 0, (NvS32)m ) != 0 )
        {
            NvOsMutexDestroy( m );
        }
    }

    NvOsMutexLock( s_Acm.Mutex );

    if (!s_Acm.Thread)
    {
        NvRmAutoClockManagerEntry *entry;

        NV_ASSERT(!s_Acm.Sem);
        NV_ASSERT(!s_Acm.ModuleSem);

        s_Acm.HostClockRef = 0;
        err = NvRmPrivMutexAlloc(&s_Acm.HostClockMutex);
        if (err)
            goto fail;

        err = NvOsSemaphoreCreate(&s_Acm.Sem, 0);
        if (err)
            goto fail;

        err = NvOsSemaphoreCreate(&s_Acm.ModuleSem, 0);
        if (err)
            goto fail;

        /* setup the sub modules */
        entry = &s_Acm.Entries[NVRM_AUTO_CLOCK_MODULE_DISPLAY];
        entry->ModuleID = NvRmModuleID_Display;

        err = NvOsThreadCreate(NvRmPrivAcmThread, hRm, &s_Acm.Thread);
        if (err)
            goto fail;
    }

    NvOsMutexUnlock( s_Acm.Mutex );

    return NvSuccess;

fail:
    NvOsMutexDestroy( s_Acm.HostClockMutex );
    NvOsSemaphoreDestroy( s_Acm.Sem );
    NvOsSemaphoreDestroy( s_Acm.ModuleSem );
    s_Acm.HostClockMutex = 0;
    s_Acm.Thread = 0;
    s_Acm.Sem = 0;
    s_Acm.ModuleSem = 0;
    NvOsMutexUnlock( s_Acm.Mutex );
    return err;
}


static void NvRmPrivAcmThread( void *args )
{
    NvRmDeviceHandle hRm = (NvRmDeviceHandle)args;
    NvU32 sleeptime = NV_WAIT_INFINITE;
    NvBool isSim = NV_FALSE;

    for (;;)
    {
        NvError err;
        err = NvOsSemaphoreWaitTimeout( s_Acm.Sem, sleeptime );
        if (s_Acm.Shutdown)
            break;

        NvOsMutexLock( s_Acm.Mutex );

        // prevent running multiple times if signalled more than once
        while (NvOsSemaphoreWaitTimeout(s_Acm.Sem, 0) == NvSuccess);

        ACMV_PRINTF(("=====       Acm WAKE t=%08x %s\n",
            NvOsGetTimeMS(),
            (err==NvError_Timeout)?"TO":"Kick"));

        //Powering host down slows graphics simulation
        // tremendously, so skip it on simulator.
        if (!isSim)
            NvRmChPrivPowerdown(hRm, NV_FALSE, &sleeptime);

        NvOsMutexUnlock( s_Acm.Mutex );
        (void)err;
    }

    /* clean up any modules that are still on */
    NvOsMutexLock( s_Acm.Mutex );
    NvRmChPrivPowerdown( hRm, NV_TRUE, &sleeptime );
    NvOsMutexUnlock( s_Acm.Mutex );
}

void NvRmPrivScheduleAcmCheck(void)
{
    NV_ASSERT(CHANNEL_IS_INITIALIZED());

    NvOsSemaphoreSignal( s_Acm.Sem );
}

void NvRmPrivHostClockEnable( NvRmDeviceHandle hRm )
{
#if NVRM_T30_QT_WAR
    if (!s_Acm.HostClockMutex)
    {
        NV_ASSERT_SUCCESS(NvRmPrivMutexAlloc(&s_Acm.HostClockMutex));
    }
#endif

    NvOsMutexLock(s_Acm.HostClockMutex);
#if !NVRM_T30_QT_WAR
    NV_ASSERT(s_Acm.HostRegistersAreValid);
#endif

    if (s_Acm.HostClockRef++ == 0)
    {
        NV_ASSERT_SUCCESS( NvRmPowerModuleClockControl( hRm,
                NvRmModuleID_GraphicsHost, g_HostPowerClientID, NV_TRUE) );
        ACM_PRINTF(("===== HOST CLOCK ON\n"));
    }
    NV_ASSERT(s_Acm.HostClockRef > 0);
    NvOsMutexUnlock(s_Acm.HostClockMutex);
}

void NvRmPrivHostClockDisable( NvRmDeviceHandle hRm )
{
#if NVRM_T30_QT_WAR
    if (!s_Acm.HostClockMutex)
    {
        NV_ASSERT_SUCCESS(NvRmPrivMutexAlloc(&s_Acm.HostClockMutex));
    }
#endif

    NvOsMutexLock(s_Acm.HostClockMutex);
#if !NVRM_T30_QT_WAR
    NV_ASSERT(s_Acm.HostRegistersAreValid);
#endif

    NV_ASSERT(s_Acm.HostClockRef > 0);
    if (--s_Acm.HostClockRef == 0)
    {
        ACM_PRINTF(("===== HOST CLOCK OFF\n"));
        NV_ASSERT_SUCCESS(NvRmPowerModuleClockControl(hRm,
            NvRmModuleID_GraphicsHost, g_HostPowerClientID, NV_FALSE));
    }
    NvOsMutexUnlock(s_Acm.HostClockMutex);
}

/**
 * Automatic clock managment -- if a module is idle for a certain amount of
 * time, then the module will be powered off. It will be powered by up
 * on the next submit. Idleness is tracked by SyncPointID/Value (when ID
 * reaches Value and there are no further submits).
 *
 * NOTE: this assumes that s_Acm.Mutex is held!
 */
static void
NvRmChModuleBusy( NvRmDeviceHandle hRm, NvRmChannelHandle hChannel,
    NvRmModuleID ModuleID, NvU32 SyncPointID, NvU32 SyncPointValue )
{
    NvError e;
    NvRmAutoClockManagerEntry *entry;
    NvU32 currenttime;
    NvU32 mod;

    NV_ASSERT( !s_Acm.Shutdown );
    NV_ASSERT(CHANNEL_IS_INITIALIZED());

    /* Record the module's newest sync point id/value. If the module is not
     * clocked, then enable the clock. The ACM thread will later check the
     * timestamp and sync point id/value to disable the clock.
     */

    mod = NVRM_MODULE_ID_MODULE( ModuleID );

    switch( mod ) {
    case NvRmModuleID_Display:
        entry = &s_Acm.Entries[NVRM_AUTO_CLOCK_MODULE_DISPLAY];
        break;
    default:
        goto clean;
    }

    currenttime = NvOsGetTimeMS();
    entry->hChannel = hChannel;
    entry->SyncPointID = SyncPointID;
    entry->SyncPointValue = SyncPointValue;
    entry->TimeStampMS = currenttime;

    /* Special case display which only cares about host clock here */
    if( mod == NvRmModuleID_Display )
    {
        entry->bClockEnabled = NV_TRUE;
    }
    else
    {
        if( !entry->PowerClientID )
        {
            entry->PowerClientID = NVRM_POWER_CLIENT_TAG('H','S','T','2');
            NV_CHECK_ERROR_CLEANUP(
                NvRmPowerRegister( hRm, 0, &entry->PowerClientID )
            );
        }

        if( !entry->bClockEnabled )
        {
            NvU32 i;

            ACM_PRINTF(("===== Mod %08x %s pwrON\n",MODPNT(entry->ModuleID)));
            NV_CHECK_ERROR_CLEANUP(
                NvRmPowerVoltageControl( hRm, entry->ModuleID,
                    entry->PowerClientID,
                    NvRmVoltsUnspecified, NvRmVoltsUnspecified,
                    NULL, 0, NULL )
            );

            NV_CHECK_ERROR_CLEANUP(
                NvRmPowerModuleClockControl( hRm, ModuleID,
                    entry->PowerClientID, NV_TRUE )
            );

            for( i = 0; entry->SubModules[i] != NvRmModuleID_Invalid; i++ )
            {
                NV_CHECK_ERROR_CLEANUP(
                    NvRmPowerVoltageControl( hRm, entry->SubModules[i],
                        entry->PowerClientID,
                        NvRmVoltsUnspecified, NvRmVoltsUnspecified,
                        NULL, 0, NULL )
                );
                NV_CHECK_ERROR_CLEANUP(
                    NvRmPowerModuleClockControl( hRm, entry->SubModules[i],
                        entry->PowerClientID, NV_TRUE )
                );
            }

            if( entry->ModuleFunc )
            {
                entry->ModuleFunc( hRm, entry, NV_TRUE );
            }

            entry->bClockEnabled = NV_TRUE;
        }
    }

clean:
fail:
    return;
}


/**
 * Increment the reference count on a bunch of NvRmMemHandles.
 * Linux nvmap always increments ref on pin, so this isn't needed.
 */
static void
IncrRefMult(
    NvRmMemHandle* handles,
    NvU32 count)
{
#if !NVOS_IS_LINUX_KERNEL
    NvRmMemHandle temp;
    /* NvRmPrivMemIncrRef is in the kernel, so it can't be called from here;
     * use NvRmMemGetId to do the increment instead.
     */
    while (count--)
        NvRmMemHandleFromId(NvRmMemGetId(*handles++), &temp);
#endif
}

/**
 * Decrement the reference count on a bunch of NvRmMemHandles.
 * Linux nvmap always decrements ref on unpin, so this isn't needed.
 */
static void
DecrRefMult(
    NvRmMemHandle* handles,
    NvU32 count)
{
#if !NVOS_IS_LINUX_KERNEL
    while (count--)
        NvRmMemHandleFree(*handles++);
#endif
}

/**
 * Sync queue
 *
 * This has two purposes:
 *   - Unpinning (& possibly freeing) of memory that was in use by HW
 *   - Updating of pushbuffer read position as gathers have completed
 *
 * The sync queue is a circular buffer of NvU32s interpreted as:
 *   0: SyncPointID
 *   1: SyncPointValue
 *   2: NumGathers (how many pushbuffer slots to free)
 *   3: NumHandles (NumRelocs + NumGathers)
 *   4..: NumHandles * NvRmMemHandle to unpin
 *
 * There's always one slot free, so (accounting for wrap):
 *   - Write == Read => queue empty
 *   - Write + 1 == Read => queue full
 * The queue must not be left with less than NVRM_SYNC_QUEUE_MIN_ENTRY words
 * of space at the end of the array.
 *
 * We want to pass contiguous arrays of handles to NrRmMemUnpin, so arrays
 * that would wrap at the end of the buffer will be split into two (or more)
 * entries.
 *
 * **All SyncQueue functions must be called with the queue's mutex held.**
 *
 * Technically, as long as only one producer calls GetFreeSpace/Add and only
 * one consumer calls Head/Dequeue, it's a classic lock-free data structure.
 * The queue's mutex was for making sure there was only one consumer (we have
 * a worker thread doing updates to the queue, but also sometimes we have to
 * wait for the queue, and that counts as another consumer).
 * Also, there's only one producer, EnqueueCleanupWork, and that's protected
 * by the channel's submit mutex.
 * In practice, SMP makes it pretty hard to get such stuff working reliably.
 * Various memory barriers are required, otherwise stuff fails in odd ways
 * at inopportune and undebuggable moments.
 * I've got an increasing awareness of the problems and how to fix them, but
 * for now this stuff needs to be rock solid, so it has gone fully mutexed :-(
 */

// Number of words needed to store an entry containing one handle
#define NVRM_SYNC_QUEUE_MIN_ENTRY 5

/**
 * Reset to empty queue.
 */
static void
SyncQueueReset(
    NvRmChSyncQueue* queue)
{
    queue->Event = NvRmChSyncQueueEvent_Nothing;
    queue->Read = 0;
    queue->Write = 0;
}

/**
 *  Find the number of handles that can be stashed in the sync queue without
 *  waiting.
 *  0 -> queue is full, must update to wait for some entries to be freed.
 */
static NvU32
SyncQueueGetFreeSpace(
    NvRmChSyncQueue* queue)
{
    NvU32 Read = queue->Read;
    NvU32 Write = queue->Write;
    NvU32 Size;

    NV_ASSERT(Read  <= (NVRM_SYNC_QUEUE_SIZE - NVRM_SYNC_QUEUE_MIN_ENTRY));
    NV_ASSERT(Write <= (NVRM_SYNC_QUEUE_SIZE - NVRM_SYNC_QUEUE_MIN_ENTRY));

    // We can use all of the space up to the end of the buffer, unless the
    // read position is within that space (the read position may advance
    // asynchronously, but that can't take space away once we've seen it).
    if (Read > Write)
        Size = (Read - 1) - Write;
    else
    {
        Size = NVRM_SYNC_QUEUE_SIZE - Write;

        // If the read position is zero, it gets complicated. We can't use the
        // last word in the buffer, because that would leave the queue empty.
        // But also if we use too much we would not leave enough space for a
        // single handle packet, and would have to wrap in SyncQueueAdd - also
        // leaving Write==Read==0, an empty queue.
        if (Read == 0)
            Size -= NVRM_SYNC_QUEUE_MIN_ENTRY;
    }

    // There must be room for an entry header and at least one handle,
    // otherwise we report a full queue.
    if (Size < NVRM_SYNC_QUEUE_MIN_ENTRY)
        return  0;
    // Minimum entry stores one handle
    return (Size - NVRM_SYNC_QUEUE_MIN_ENTRY) + 1;
}

/**
 * Add an entry to the sync queue.
 */
static void
SyncQueueAdd(
    NvRmChSyncQueue* queue,
    NvU32 SyncPointID,
    NvU32 SyncPointValue,
    NvU32 NumGathers,
    NvRmMemHandle* Handles,
    NvU32 NumHandles)
{
    NvU32 Write = queue->Write;
    NvU32* p = queue->Buffer + Write;
    NvU32 Size = 4 + NumHandles;

    NV_ASSERT(SyncPointID != NVRM_INVALID_SYNCPOINT_ID);
    NV_ASSERT(NumHandles);
    NV_ASSERT(SyncQueueGetFreeSpace(queue) >= NumHandles);

    Write += Size;
    NV_ASSERT(Write <= NVRM_SYNC_QUEUE_SIZE);

    *p++ = SyncPointID;
    *p++ = SyncPointValue;
    *p++ = NumGathers;
    *p++ = NumHandles;
    NvOsMemcpy(p, Handles, NumHandles * sizeof(NvU32));
    p += NumHandles;

    // If there's not enough room for another entry, wrap to the start.
    if ((Write + NVRM_SYNC_QUEUE_MIN_ENTRY) > NVRM_SYNC_QUEUE_SIZE)
    {
        // It's an error for the read position to be zero, as that would mean
        // we emptied the queue while adding something.
        NV_ASSERT(queue->Read != 0);
        Write = 0;
    }

    queue->Write = Write;
}

/**
 * Get a pointer to the next entry in the queue, or NULL if the queue is empty.
 * Doesn't consume the entry.
 */
static NvU32* SyncQueueHead(
    NvRmChSyncQueue* queue)
{
    NvU32 Read = queue->Read;
    NvU32 Write = queue->Write;

    NV_ASSERT(Read  <= (NVRM_SYNC_QUEUE_SIZE - NVRM_SYNC_QUEUE_MIN_ENTRY));
    NV_ASSERT(Write <= (NVRM_SYNC_QUEUE_SIZE - NVRM_SYNC_QUEUE_MIN_ENTRY));

    if (Read == Write)
        return NULL;
    return queue->Buffer + Read;
}

/**
 * Advances to the next queue entry, if you want to consume it.
 */
static void SyncQueueDequeue(
    NvRmChSyncQueue* queue)
{
    NvU32 Read = queue->Read;
    NvU32 Size;

    NV_ASSERT(Read != queue->Write);

    Size = 4 + queue->Buffer[Read + 3];
    NV_ASSERT(((Read + Size) <= queue->Write) || (queue->Write < Read));

    Read += Size;
    NV_ASSERT(Read <= NVRM_SYNC_QUEUE_SIZE);

    // If there's not enough room for another entry, wrap to the start.
    if ((Read + NVRM_SYNC_QUEUE_MIN_ENTRY) > NVRM_SYNC_QUEUE_SIZE)
        Read = 0;

    queue->Read = Read;
}

/**
 * For all sync queue entries that have already finished according to the
 * current sync point registers:
 *  - unpin & unref their hMems
 *  - bump the pushbuffer fence if they're gathers
 *  - remove them from the sync queue
 * This is normally called from the host code's worker thread, but can be
 * called manually if necessary.
 * The sync queue's mutex must be released before calling this function.
 */
void NvRmPrivUpdateSyncQueue(NvRmChannelHandle hChannel)
{
    NvRmChHw *hw = hChannel->HwState;
    NvRmChSyncQueue* queue = &hw->sync_queue;
    NvU32 LastSyncPointID = NVRM_INVALID_SYNCPOINT_ID;
    NvU32 LastSyncPointValue = 0;
    NvBool Signal = NV_FALSE;

    NvOsMutexLock(queue->Mutex);

    // Walk the sync queue, reading the sync point registers as necessary, to
    // consume as many sync queue entries as possible without blocking
    for (;;)
    {
        NvU32 SyncPointID, SyncPointValue, NumGathers, NumHandles;
        NvRmMemHandle* Handles;
        NvU32 *sync;

        sync = SyncQueueHead(queue);
        if (!sync)
        {
            if (queue->Event == NvRmChSyncQueueEvent_Empty)
                Signal = NV_TRUE;
            break;
        }

        SyncPointID = sync[0];
        NV_ASSERT(SyncPointID != NVRM_INVALID_SYNCPOINT_ID);

        // Check whether this syncpt has completed, and bail if not
        // Don't read the same syncpt register multiple times in a row
        // This is still suboptimal in terms of register reads; we should
        // really have a global (RM-wide) sync pt register value cache.
        if (LastSyncPointID != SyncPointID)
        {
            LastSyncPointID = SyncPointID;
            LastSyncPointValue =
                NvRmChPrivSyncPointRead(hChannel->hDevice, SyncPointID);
        }

        /* check for wrap-around */
        SyncPointValue = sync[1];
        if ((NvS32)(LastSyncPointValue - SyncPointValue) < 0)
            break;

        NumGathers = sync[2];
        NumHandles = sync[3];
        Handles = (NvRmMemHandle*)(sync + 4);

        // Unpin the memory -- gather vs. reloc makes no difference here
        NvRmMemUnpinMult(Handles, NumHandles);
        DecrRefMult(Handles, NumHandles);

        // Advance the PB fence -- each gather command is 8 bytes.
        // Note that NVRM_PUSHBUFFER_SIZE is a power of 2.
        if (NumGathers)
        {
            hw->fence = (hw->fence + NumGathers * 8) & (NVRM_PUSHBUFFER_SIZE - 1);
            if (queue->Event == NvRmChSyncQueueEvent_PushBuffer)
                Signal = NV_TRUE;
        }

        SyncQueueDequeue(queue);
        if (queue->Event == NvRmChSyncQueueEvent_SyncQueue)
            Signal = NV_TRUE;
    }

    // Wake up NvRmPrivWaitSyncQueue() if the requested event happened
    if (Signal)
    {
        queue->Event = NvRmChSyncQueueEvent_Nothing;
        NvOsSemaphoreSignal(queue->Semaphore);
    }

    NvOsMutexUnlock(queue->Mutex);
}

/**
 * Helper function, returns the status of the channel's sync queue or push
 * buffer (which is managed by the sync queue) for the given event.
 *  - Empty: returns 1 for empty, 0 for not empty (as in "1 empty queue" :-)
 *  - SyncQueue: returns the number of handles that can be stored in the queue
 *  - PushBuffer: returns the number of free slots in the channel's push buffer
 * Must be called with the queue mutex held.
 */
static NvU32
GetSyncQueueStatus(
    NvRmChHw* hw,
    NvRmChSyncQueueEvent event)
{
    switch (event)
    {
        case NvRmChSyncQueueEvent_Empty:
            return SyncQueueHead(&hw->sync_queue) ? 0 : 1;

        case NvRmChSyncQueueEvent_SyncQueue:
            return SyncQueueGetFreeSpace(&hw->sync_queue);

        case NvRmChSyncQueueEvent_PushBuffer:
            return ((hw->fence - hw->current) & (NVRM_PUSHBUFFER_SIZE - 1)) / 8;

        default:
            NV_ASSERT(0);
            return 0;
    }
}

/**
 * Sleep (if necessary) until the requested event happens
 *   - Empty : queue is completely empty.
 *     - Returns 1
 *   - SyncQueue : there is space in the sync queue.
 *   - PushBuffer : there is space in the push buffer
 *     - Return the amount of space (> 0)
 * Must be called with the queue mutex held; returns with it still held
 */
static NvU32
NvRmPrivWaitSyncQueue(
    NvRmChHw* hw,
    NvRmChSyncQueueEvent event)
{
    NvRmChSyncQueue* queue = &hw->sync_queue;
    NvU32 Space = GetSyncQueueStatus(hw, event);

    if (!Space)
    {
        NV_ASSERT(queue->Event == NvRmChSyncQueueEvent_Nothing);
        queue->Event = event;

        NvOsMutexUnlock(queue->Mutex);
        NvOsSemaphoreWait(queue->Semaphore);
        NvOsMutexLock(queue->Mutex);

        NV_ASSERT(queue->Event == NvRmChSyncQueueEvent_Nothing);

        Space = GetSyncQueueStatus(hw, event);
        NV_ASSERT(Space);
    }
    return Space;
}

/**
 * Wait for every entry in the sync queue (all gathers and relocs) to complete.
 * Reset gather fifo.  This will be slow!
 */
static void
NvRmPrivChannelFlush( NvRmChannelHandle hChannel )
{
    HOST_CLOCK_ENABLE( hChannel->hDevice );

    NvOsMutexLock(hChannel->HwState->sync_queue.Mutex);
    (void)NvRmPrivWaitSyncQueue(hChannel->HwState, NvRmChSyncQueueEvent_Empty);
    NvOsMutexUnlock(hChannel->HwState->sync_queue.Mutex);

    NvRmPrivChannelReset(hChannel);

    HOST_CLOCK_DISABLE( hChannel->hDevice );
}

/**
 * Init a PinBlock, given a block of memory and a capacity
 * The HMem array holds NvRmMemHandles to be pinned
 * The Phys array holds the result of NvRmMemPinMult for each hMem
 * The Index array maps from original reloc index to unique hMem subset
 * Actually, there's only one array with three pointers into it.
 */
static void
PinBlockInit(
    NvRmChPinBlock* pb,
    NvU32* Mem,
    NvU32 Capacity)
{
    pb->HMem = (NvRmMemHandle*)Mem;
    pb->Phys = Mem + Capacity;
    pb->Index = pb->Phys + Capacity;
    pb->Capacity = Capacity;
}

/**
 * Ensure there's enough space in the PinBlock arrays for the requested number
 * of handles.
 * In reality the only user of the channel stuff is NvRmStream, which will
 * never submit more than 1280 handles at a time (until someone changes it...)
 * That's 15KiB for the PinBlock, per channel in use.
 */
static NvError
PinBlockEnsureCapacity(
    NvRmChPinBlock* pb,
    NvU32 Needed)
{
    NvU32* hMem;
    if (pb->Capacity >= Needed)
        return NvSuccess;
    NvOsFree(pb->HMem);
    if (Needed == 0)
    {
        PinBlockInit(pb, NULL, 0);
        return NvSuccess;
    }
    hMem = NvOsAlloc(Needed * 3 * 4);
    PinBlockInit(pb, hMem, Needed);
    return hMem ? NvSuccess : NvError_InsufficientMemory;
}

/**
 * Find a subset of unique target hMems from the given relocations,
 * and create an map from original index to unique hMem.
 * Actually it's simple adjacent duplicate detection, most useful for VBOs
 */
static void
PinBlockProcessRelocations(
    NvRmChPinBlock* pb,
    const NvRmChannelSubmitReloc* reloc,
    NvU32 count)
{
    NvRmMemHandle prevhMem = NULL;
    NvU32 unique = (NvU32)-1;
    NvU32 i;
    for (i = 0; i < count; ++i)
    {
        NvRmMemHandle h = reloc[i].hMemTarget;
        if (h != prevhMem)
        {
            prevhMem = h;
            pb->HMem[++unique] = h;
        }
        pb->Index[i] = unique;
    }
    pb->Count = unique + 1;
}

/**
 * Add command buffer hMems to the PinBlock.
 * Doesn't bother doing duplicate detection.
 * If there were a lot of gathers, there would be one main command buffer that
 * should be detected. It would probably be the first one, unless the submit
 * started with a gather. TODO?
 */
static void
PinBlockAddCommandBuffers(
    NvRmChPinBlock* pb,
    const NvRmCommandBuffer* pCommandBufs,
    NvU32 NumCommandBufs)
{
    NvRmMemHandle* p = pb->HMem;
    NvU32 Count = pb->Count;
    p += Count;
    pb->Count = Count + NumCommandBufs;
    NV_ASSERT(pb->Count <= pb->Capacity);
    while (NumCommandBufs--)
        *p++ = (pCommandBufs++)->hMem;
}

#if NVOS_IS_LINUX_KERNEL
// Command buffer is already mapped into kernel space (for now)
#define MIN_RELOCATIONS_FOR_MAP 0
#else
// [AnttiR]
// TODO:
// I just picked a number that feels good. Needs to be adjusted
// when the code is in kernel. Generally the optimal value should
// be x in equation:
//
// COST_MEMMAP = x * COST_MEMWR32
//
#define MIN_RELOCATIONS_FOR_MAP 10
#endif

/**
 * Patch relocations into command buffers.
 * PinBlockPhys is a subset of the original target hMems
 * PinBlockIndex is a remapping table from original reloc index to PinBlockPhys
 */
static void
PatchRelocations(
    const NvRmChannelSubmitReloc *pRelocations,
    const NvU32 *pRelocShifts,
    NvU32 NumRelocations,
    NvU32* PinBlockPhys,
    NvU32* PinBlockIndex)
{
    NvU32 *pCmdBufMapped = NULL;
    NvRmMemHandle mappedhMem = NULL;
    const NvRmChannelSubmitReloc *r = pRelocations;
    const NvU32 *rShift = pRelocShifts;
    NvU32 i;

    // TODO: Gary's nvmap voodoo, when/if kernel side map is removed

    // Optimization:
    // Use NvRmMemMap instead of NvRmMemWr32.
    // Based on assumption that each reloc entry has the same
    // hCmdBufMem. This is true in NvRmStream implementation.
    if (NumRelocations > MIN_RELOCATIONS_FOR_MAP)
    {
        // FIXME: Assumes NVRM_CMDBUF_SIZE 32768
        if (NvRmMemMap(pRelocations->hCmdBufMem,
                       0, 32768, NVOS_MEM_WRITE,
                       (void*)&pCmdBufMapped) == NvSuccess)
            mappedhMem = pRelocations->hCmdBufMem;
    }

    for (i = 0; i < NumRelocations; ++i, ++r, ++rShift)
    {
        NvU32 phys = (PinBlockPhys[PinBlockIndex[i]] + r->TargetOffset) >> *rShift;
        // Fall back to MemWr32 if hMem assumption above was wrong.
        if (mappedhMem == r->hCmdBufMem)
            pCmdBufMapped[r->CmdBufOffset/4] = phys;
        else
            NvRmMemWr32(r->hCmdBufMem, r->CmdBufOffset, phys);
    }

    // FIXME: Assumes NVRM_CMDBUF_SIZE 32768
    if (mappedhMem)
        NvRmMemUnmap(mappedhMem, pCmdBufMapped, 32768);
}

/**
 * Kick off some command buffers (usually one, unless there are stream gathers
 * or context switches)
 *
 * We don't need to put our own buffers into the sync queue yet. None of them
 * will leave the queue until the last one (containing the sync point increment)
 * has been kicked off. So if the push buffer is full, it's something already
 * in the sync queue that we need to wait for.
 */
static void
KickOffCommandBuffers(
    NvRmChannelHandle hChannel,
    const NvRmCommandBuffer *pCommandBufs,
    NvU32 NumCommandBufs,
    NvU32* CmdBufPhys,
    NvBool NullKickoff)
{
    NvRmChHw *hw = hChannel->HwState;
    const NvRmCommandBuffer *cmd = pCommandBufs;
    NvU32 i;

    for (i = 0; i < NumCommandBufs; ++i, ++cmd)
    {
        NvU32 phys, op, put;

        // Ensure that there's enough room in the pushbuffer to write a gather
        (void)NvRmPrivWaitSyncQueue(hw, NvRmChSyncQueueEvent_PushBuffer);

        // Write the gather to the HW and write the channel Put pointer.  Note
        // that in the wrap case, we write Put to 0, not NVRM_PUSHBUFFER_SIZE;
        // this allows the hardware to process the RESTART command slightly
        // sooner.  We could perhaps get away with writing Put less often, but
        // it shouldn't cost much to write it.
        if (!NullKickoff)
        {
            op = NVOPCODE_GATHER(0, DISABLE, NONINCR, cmd->Words);
            phys = CmdBufPhys[i] + cmd->Offset;
        }
        else
        {
            /* write 2 no-ops to hardware */
            op = NVOPCODE_NOOP;
            phys = op;
        }

        if (hw->pb_mem)
        {
            NV_WRITE32((NvUPtr)hw->pb_mem + hw->current, op);
            NV_WRITE32((NvUPtr)hw->pb_mem + hw->current + 4, phys);
        }
        else
        {
            NvRmMemWr32(hw->pushbuffer, hw->current, op);
            NvRmMemWr32(hw->pushbuffer, hw->current + 4, phys);
        }

        // prevent a race between the memory system and the channel hardware.
        NvOsFlushWriteCombineBuffer();

        // calculate the PUT pointer and tell the hardware to fetch the data
        hw->current = (hw->current + 8) & (NVRM_PUSHBUFFER_SIZE - 1);
        put = hw->pb_addr + hw->current;

        CHANNEL_REGW(hw, DMAPUT, put);
    }
}

/**
 * Add a contiguous block of memory handles to the sync queue, and a number
 * of gathers to be freed from the pushbuffer.
 * Blocks as necessary if the sync queue is full.
 * The handles for a submit must all be pinned at the same time, but they
 * can be unpinned in smaller chunks.
 */
static void
EnqueueCleanupWork(
    NvRmChannelHandle hChannel,
    NvU32 SyncPointID,
    NvU32 SyncPointValue,
    NvU32 NumGathers,
    NvRmMemHandle* Handles,
    NvU32 NumHandles)
{
    NvRmChHw *hw = hChannel->HwState;
    NvRmChSyncQueue* queue = &hw->sync_queue;
    while (NumHandles)
    {
        // Wait until there's enough room in the sync queue to write something.
        NvU32 Count = NvRmPrivWaitSyncQueue(hw, NvRmChSyncQueueEvent_SyncQueue);

        // Add reloc entries to sync queue (as many as will fit) and unlock it
        if (Count > NumHandles)
            Count = NumHandles;
        SyncQueueAdd(queue, SyncPointID, SyncPointValue,
                     NumGathers, Handles, Count);
        NumGathers = 0; // NumGathers only goes in the first packet
        Handles += Count;
        NumHandles -= Count;
    }
}

/**
 * Does the main work of submitting command buffers.
 * Must have channel submit mutex.
 */
void
NvRmPrivChannelSubmitCommandBufs( NvRmChannelHandle hChannel,
    const NvRmCommandBuffer *pCommandBufs, NvU32 NumCommandBufs,
    const NvRmChannelSubmitReloc *pRelocations, const NvU32 *pRelocShifts,
    NvU32 NumRelocations,
    NvU32 SyncPointID, NvU32 SyncPointValue, NvBool NullKickoff )
{
    NvRmChHw* hw = hChannel->HwState;
    NvRmChPinBlock* pb = &hw->PinBlock;
    NvU32* CmdBufPhys;
    NvError err;

    NV_ASSERT(NumCommandBufs > 0);
    NV_ASSERT(SyncPointID != NVRM_INVALID_SYNCPOINT_ID);

    // Make a vector of reloc target hMems to pin, removing some duplicates
    PinBlockProcessRelocations(pb, pRelocations, NumRelocations);

    // Add the command buffer hMems to the PinBlock
    CmdBufPhys = pb->Phys + pb->Count;
    PinBlockAddCommandBuffers(pb, pCommandBufs, NumCommandBufs);

    // Ref and pin all hMems. The pin must be atomic for GART virtualization.
    IncrRefMult(pb->HMem, pb->Count);
    NvRmMemPinMult(pb->HMem, pb->Phys, pb->Count);

    // Now we have physical addresses for all hMems, patch up relocations
    PatchRelocations(pRelocations,
                     pRelocShifts,
                     NumRelocations,
                     pb->Phys, pb->Index);

    NvOsMutexLock(hw->sync_queue.Mutex);

    // Send gathers to HW
    KickOffCommandBuffers(hChannel, pCommandBufs, NumCommandBufs,
                          CmdBufPhys, NullKickoff);

    // Stash pinned hMems and gather count into sync queue for later cleanup
    EnqueueCleanupWork(hChannel, SyncPointID, SyncPointValue,
                       NumCommandBufs, pb->HMem, pb->Count);

    NvOsMutexUnlock(hw->sync_queue.Mutex);

    // Schedule a callback to NvRmPrivUpdateSyncQueue when the submit completes
    err = NvRmPrivSubmitSchedule(hChannel->hDevice,
                                 SyncPointID, SyncPointValue, hChannel);
    // If that failed, spin until the sync queue's empty
    if (err != NvSuccess)
    {
        NvOsMutexLock(hw->sync_queue.Mutex);
        while (SyncQueueHead(&hw->sync_queue))
        {
            NvOsMutexUnlock(hw->sync_queue.Mutex);
            // The sync queue is normally updated from another thread.
            // It's OK for us to update it too, but we must release our mutex
            // first. The other thread may still be doing previous updates,
            // we just couldn't schedule ours to happen.
            NvRmPrivUpdateSyncQueue(hChannel);
            NvOsMutexLock(hw->sync_queue.Mutex);
        }
        NvOsMutexUnlock(hw->sync_queue.Mutex);
    }
}

NvError
NvRmChannelSubmit( NvRmChannelHandle hChannel,
    const NvRmCommandBuffer *pCommandBufs, NvU32 NumCommandBufs,
    const NvRmChannelSubmitReloc *pRelocations,
    const NvU32 *pRelocShifts,
    NvU32 NumRelocations,
    const NvRmChannelWaitChecks *pWaitChecks,
    NvU32 NumWaitChecks,
    NvRmContextHandle hContext,
    const NvU32 *pContextExtra, NvU32 ContextExtraCount,
    NvBool NullKickoff, NvRmModuleID ModuleID,
    NvU32 StreamSyncPointIdx, NvU32 SyncPointWaitMask,
    NvU32 *pCtxChanged,
    NvU32 *pSyncPointValue,
    NvRmSyncPointDescriptor *SyncPointIncrs,
    NvU32 NumSyncPoints)
{
    NvU32 SyncPointID = SyncPointIncrs[StreamSyncPointIdx].SyncPointID;
    NvU32 SyncPointsUsed = SyncPointIncrs[StreamSyncPointIdx].Value;
    NvU32 OldSyncPointValue;
    NvRmChHw *hw;
    NvU32 val = 0;
    NvU32 used = 0;
    NvBool signalAcm = NV_FALSE;

    NV_ASSERT( hChannel );
    NV_ASSERT( hChannel->HwState );
    NV_ASSERT( pCommandBufs );
    NV_ASSERT( NumCommandBufs );
    NV_ASSERT( NumCommandBufs <= NVRM_GATHER_QUEUE_SIZE-1 );
    NV_ASSERT( (NumCommandBufs + NumRelocations)
               <= NVRM_CHANNEL_SUBMIT_MAX_HANDLES );
    hw = hChannel->HwState;

    /* always lock the ACM mutex before the channel submit mutex. this ensures
     * that the module can't be disabled before the submit makes it to
     * hardware (which happens when single stepping with a debugger).
     */
    NvOsMutexLock( s_Acm.Mutex );

    NV_ASSERT(!s_Acm.HostIsSuspended);

    /* lock the channel: note that the channel's lock will be held even during
     * the wait for sync point.
     */
    NvOsMutexLock( hw->submit_mutex );

#if CHANNEL_DUMP
    NvRmPrivDumpCommandBuffers( hw->num, pCommandBufs, NumCommandBufs );
#endif

    *pCtxChanged = 0;

    /* some sync points are totally owned by the clients. don't use the shadow
     * in this case. SyncPointsUsed is the absolute value of the sync point
     */
    if( (1UL << SyncPointID) & NVRM_SYNCPOINTS_CLIENT_MANAGED )
    {
        val = SyncPointsUsed;
        /* FIXME: this keeps gcc quiet, but doesn't necessarily make sense
         * for the following context change check.  As long as only display
         * uses client managed sync-points, this may be fine.
         */
        OldSyncPointValue = 0;
    }
    else
    {
        used = SyncPointsUsed;
        NvRmPrivIncrSyncPointShadow( hChannel->hDevice,
            SyncPointID, SyncPointsUsed, &OldSyncPointValue, &val );

        SyncPointsUsed =
            NvRmPrivCheckForContextChange( hChannel, hContext, ModuleID,
                SyncPointID, OldSyncPointValue, NV_TRUE );
        if (SyncPointsUsed)
        {
            *pCtxChanged = 1;
            used += SyncPointsUsed;
            NvRmPrivIncrSyncPointShadow( hChannel->hDevice,
                SyncPointID, SyncPointsUsed, NULL, &val );
        }
    }

    if( pSyncPointValue )
    {
        *pSyncPointValue = val;
    }

    ACMV_PRINTF(("=====       Submit mod %08x %s\n",MODPNT(ModuleID)));

    /* If s_Acm.ModulesBusy then Acm is running and host clock is already
     * enabled.
     */
    if ( !s_Acm.ModulesBusy )
    {
        NvRmChPrivHostIsBusy( hChannel->hDevice );

        ACM_PRINTF(("===== BUSY: Submit mod %08x %s\n",MODPNT(ModuleID)));

        // wait until we release s_Acm.Mutex before signalling the Acm thread
        // to avoid extra context switches.
        signalAcm = NV_TRUE;
    }

    NvRmChModuleBusy( hChannel->hDevice, hChannel, ModuleID,
        SyncPointID, val );

    (void)NvRmPrivCheckForContextChange( hChannel, hContext, ModuleID,
        SyncPointID, OldSyncPointValue, NV_FALSE );

    NvRmPrivChannelSubmitCommandBufs( hChannel,
        pCommandBufs, NumCommandBufs,
        pRelocations, pRelocShifts, NumRelocations,
        SyncPointID, val,
        NullKickoff );

    if( NullKickoff )
    {
        /* artifically increment the syncpoint since the hardware won't see the
         * commands.
         */
        while( used-- )
        {
            /* write the syncpt_cpu_incr register -- don't touch the shadow */
            NvRmChPrivIncrSyncpt(SyncPointID);
        }
    }

    NvOsMutexUnlock( hw->submit_mutex );

    NvOsMutexUnlock( s_Acm.Mutex );

    if (signalAcm)
        NvOsSemaphoreSignal( s_Acm.Sem );

    return NvSuccess;
}

NvError
NvRmChannelGetModuleClockRate(
    NvRmChannelHandle hChannel,
    NvRmModuleID Module,
    NvU32 *Rate)
{
    /* Should we return NvError_NotSupported instead */
    return NvSuccess;
}

NvError
NvRmChannelSetModuleClockRate(
    NvRmChannelHandle hChannel,
    NvRmModuleID Module,
    NvU32 Rate)
{
    /* Should we return NvError_NotSupported instead */
    return NvSuccess;
}

NvError
NvRmChannelSetModuleBandwidth(
    NvRmChannelHandle hChannel,
    NvRmModuleID Module,
    NvU32 BW)
{
    /* Should we return NvError_NotSupported instead */
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
    return NvError_NotSupported;
}

NvError
NvRmContextAlloc( NvRmDeviceHandle hDevice, NvRmModuleID Module,
    NvRmContextHandle *phContext )
{
    return NvError_NotImplemented;
}

NvError
NvRmPrivPrepareForPowerdown( NvRmChannelHandle hChannel,
    NvRmModuleID ModuleID, NvU32 *SyncPointID, NvU32 *SyncPointValue )
{
    NvU32 OldSyncPointValue;
    NvU32 val;
    NvRmContextHandle hContext = NVRM_CHANNEL_CONTEXT_GET(hChannel, ModuleID);
    NvU32 SyncPointsUsed;

    if (!hContext)
        return NvError_NotInitialized;
#define SYNCPT_USED_BY_CONTEXT_CHANGE 3
    NvRmPrivIncrSyncPointShadow( hChannel->hDevice,
        hContext->SyncPointID, SYNCPT_USED_BY_CONTEXT_CHANGE,
        &OldSyncPointValue, &val );

    NvOsMutexLock(hChannel->HwState->submit_mutex);
    SyncPointsUsed =
        NvRmPrivCheckForContextChange( hChannel,
            NVRM_FAKE_POWERDOWN_CONTEXT, ModuleID,
            hContext->SyncPointID, OldSyncPointValue, NV_FALSE );
    NvOsMutexUnlock(hChannel->HwState->submit_mutex);
    // We could get the actual value by calling NvRmPrivCheckForContextChange
    // with DryRun parameter set to NV_TRUE.  It should always return 2
    // though, if it's not, make sure it's doing what you think it is.
    NV_ASSERT(SyncPointsUsed == SYNCPT_USED_BY_CONTEXT_CHANGE);

    (void)SyncPointsUsed;

    *SyncPointID = hContext->SyncPointID;
    *SyncPointValue = val;
    return NvSuccess;
}

void
NvRmContextFree(NvRmContextHandle hContext)
{
    return;
}

void
NvRmPrivResetModuleSyncPoints( NvRmDeviceHandle hDevice )
{
    NvU32 i;

    for( i = 0; i < NV_HOST1X_SYNCPT_NB_PTS; i++ )
    {
        if( (1UL << i) & NVRM_SYNCPOINTS_RESERVED )
        {
            NvRmPrivResetSyncpt( hDevice, i );
        }
    }
}

void
NvRmPrivResetSyncpt( NvRmDeviceHandle hDevice, NvU32 id )
{
    NvU32 idx;
    NvU32 reg;

    /* each sync point int mask field is 2 bits */
    idx = id / 16;
    reg = g_channelState.SyncPointShadow.thresh_int_mask[idx];
    reg = reg & ~(3UL << ((id%16)* 2));
    g_channelState.SyncPointShadow.thresh_int_mask[idx] = reg;
    HOST1X_SYNC_REGW( HOST1X_SYNC_SYNCPT_THRESH_INT_MASK_0_0 +
        (idx * 4), reg );

    g_channelState.SyncPointShadow.syncpt[id] = 0;
    HOST1X_SYNC_REGW( HOST1X_SYNC_SYNCPT_0_0 + (id * 4), 0 );
    HOST1X_SYNC_REGW( HOST1X_SYNC_SYNCPT_INT_THRESH_0_0 +
        (id * 4), 0 );
}

NvError
NvRmChannelSyncPointAlloc(NvRmDeviceHandle hDevice, NvU32 *pSyncPointID)
{
    NvU32 val;
    NvU16 i;

    NV_ASSERT( hDevice );

    NvOsMutexLock( g_channelMutex );

    val = g_channelState.SyncPointVector;
    if( val == 0xffffffff )
    {
        NvOsMutexUnlock( g_channelMutex );
        return NvError_RmSyncPointAllocFailure;
    }

    // Unlock g_channelMutex first to avoid deadlock which may occur if
    // NvRmChannelSubmit is called by another thread.
    NvOsMutexUnlock( g_channelMutex );

    // FIXME: this is really slow.
    /* flush all channels to prevent state managment issues. need to do this
     * in 3 steps: 1) lock all channels 2) flush the channels
     * 3) unlock channels.
     */
    for( i = g_channelState.BaseChannel; i < g_channelState.NumChannels; i++ )
    {
        NvOsMutexLock( g_channelState.Channels[i].HwState->ref_mutex );
        if( g_channelState.Channels[i].HwState->refcount )
        {
            NvOsMutexLock( g_channelState.Channels[i].HwState->submit_mutex );
        }
    }

    // re-lock g_channelMutex
    NvOsMutexLock( g_channelMutex );

    for( i = g_channelState.BaseChannel; i < g_channelState.NumChannels; i++ )
    {
        if( g_channelState.Channels[i].HwState->refcount )
        {
            NvRmPrivChannelFlush( &g_channelState.Channels[i] );
        }
    }
    for( i = g_channelState.BaseChannel; i < g_channelState.NumChannels; i++ )
    {
        if( g_channelState.Channels[i].HwState->refcount )
        {
            NvOsMutexUnlock(
                g_channelState.Channels[i].HwState->submit_mutex );
        }

        NvOsMutexUnlock( g_channelState.Channels[i].HwState->ref_mutex );
    }

    i = (NvU16)NvULowestBitSet( ~(val), 32 );
    *pSyncPointID = i;
    g_channelState.SyncPointVector |= (1UL << i);

    /* reset the sync point */
    HOST_CLOCK_ENABLE( hDevice );

    NvRmPrivResetSyncpt( hDevice, i );

    HOST_CLOCK_DISABLE( hDevice );

    NvOsMutexUnlock( g_channelMutex );

    return NvSuccess;
}

void
NvRmChannelSyncPointFree(NvRmDeviceHandle hDevice, NvU32 SyncPointID)
{
    NV_ASSERT( hDevice );

    if( SyncPointID == NVRM_INVALID_SYNCPOINT_ID )
    {
        return;
    }

    NV_ASSERT( SyncPointID < NV_HOST1X_SYNCPT_NB_PTS );

    if( (1UL << SyncPointID) & NVRM_SYNCPOINTS_RESERVED )
    {
        return;
    }

    NvOsMutexLock( g_channelMutex );
    g_channelState.SyncPointVector &= ~(1UL << (NvU16)SyncPointID);
    NvOsMutexUnlock( g_channelMutex );
}

NvError
NvRmChannelSyncPointReadMax(
    NvRmDeviceHandle hDevice,
    NvU32 SyncPointID,
    NvU32 *Value)
{
    *Value = g_channelState.SyncPointShadow.syncpt[SyncPointID];
    return NvSuccess;
}

NvU32
NvRmChannelSyncPointRead(NvRmDeviceHandle hDevice, NvU32 SyncPointID)
{
    /* do not update the shadow.  the hardware will probably be lagging
     * behind the shadow value.
     */
    NvU32   ret;
    HOST_CLOCK_ENABLE( hDevice );
    ret = NvRmChPrivSyncPointRead( hDevice, SyncPointID );
    HOST_CLOCK_DISABLE( hDevice );
    return ret;
}

static NvU32
NvRmChPrivSyncPointRead(NvRmDeviceHandle hDevice, NvU32 SyncPointID)
{
    NV_ASSERT( hDevice );
    NV_ASSERT( SyncPointID < NV_HOST1X_SYNCPT_NB_PTS );

    /* do not update the shadow.  the hardware will probably be lagging
     * behind the shadow value.
     */
    return HOST1X_SYNC_REGR( HOST1X_SYNC_SYNCPT_0_0 +
        (SyncPointID * 4) );
}

static void
NvRmChPrivSyncPointIncr(NvRmDeviceHandle hDevice, NvU32 SyncPointID)
{
    NV_ASSERT( hDevice );
    NV_ASSERT( SyncPointID < NV_HOST1X_SYNCPT_NB_PTS );
    NV_ASSERT( g_channelState.Host1xSyncBase );

    /* increment shadow */
    NvOsMutexLock( g_channelMutex );
    g_channelState.SyncPointShadow.syncpt[SyncPointID]++;
    NvOsMutexUnlock( g_channelMutex );
    /* write the syncpt_cpu_incr register */
    NvRmChPrivIncrSyncpt(SyncPointID);
}

NvError
NvRmFenceTrigger(
    NvRmDeviceHandle hDevice,
    const NvRmFence *Fence)
{
#if NV_DEBUG
    NvU32 cur;
#endif
    HOST_CLOCK_ENABLE( hDevice );
#if NV_DEBUG
    cur = NvRmChannelSyncPointRead(hDevice, Fence->SyncPointID);
    NV_ASSERT(Fence->Value == cur+1);
#endif

    NvRmChPrivSyncPointIncr(hDevice, Fence->SyncPointID);
    HOST_CLOCK_DISABLE( hDevice );

    return NvSuccess;
}

void
NvRmChannelSyncPointIncr(NvRmDeviceHandle hDevice, NvU32 SyncPointID)
{
    HOST_CLOCK_ENABLE( hDevice );
    NvRmChPrivSyncPointIncr(hDevice, SyncPointID);
    HOST_CLOCK_DISABLE( hDevice );
}

/* Define the following macro to 1, if you want to enable poll for the
 * syncpoints instead of waiting on interrupts
 */
#ifndef HOST1X_POLLING_MODE
#define HOST1X_POLLING_MODE 0
#endif

static NvBool
NvRmChPrivSyncPointSignalSemaphore( NvRmDeviceHandle hDevice,
    NvU32 SyncPointID, NvU32 Threshold, NvOsSemaphoreHandle hSema)
{
    NvU32 val;

    NV_ASSERT( hDevice );
    NV_ASSERT( SyncPointID < NV_HOST1X_SYNCPT_NB_PTS );

#if HOST1X_POLLING_MODE
    do
    {
        val = NvRmChPrivSyncPointRead( hDevice, SyncPointID );
    } while ((NvS32)(val - Threshold) < 0);
#else
    val = NvRmChPrivSyncPointRead( hDevice, SyncPointID );
#endif

    if ((NvS32)(val - Threshold) >= 0)
    {
        return NV_TRUE;
    }

    // FIXME: how to report errors?
    (void)NvRmPrivSyncPointSchedule( hDevice, SyncPointID, Threshold, hSema );
    return NV_FALSE; // will be signaled later
}

NvBool
NvRmFenceSignalSemaphore(
    NvRmDeviceHandle hDevice,
    const NvRmFence *Fence,
    NvOsSemaphoreHandle hSema)
{
    // FIXME: this function is deprecated. remove it. don't remove
    // NvRmChPrivSyncPointSignalSemaphore though.

    NvBool  ret;
    HOST_CLOCK_ENABLE( hDevice );
    ret = NvRmChPrivSyncPointSignalSemaphore(
            hDevice, Fence->SyncPointID, Fence->Value, hSema);
    HOST_CLOCK_DISABLE( hDevice );
    return ret;
}

NvBool
NvRmChannelSyncPointSignalSemaphore( NvRmDeviceHandle hDevice,
    NvU32 SyncPointID, NvU32 Threshold, NvOsSemaphoreHandle hSema)
{
    // FIXME: this function is deprecated. remove it. don't remove
    // NvRmChPrivSyncPointSignalSemaphore though.

    NvBool  ret;
    HOST_CLOCK_ENABLE( hDevice );
    ret = NvRmChPrivSyncPointSignalSemaphore(
            hDevice, SyncPointID, Threshold, hSema);
    HOST_CLOCK_DISABLE( hDevice );
    return ret;
}

void
NvRmChannelSyncPointWait( NvRmDeviceHandle hDevice,
    NvU32 SyncPointID, NvU32 Threshold, NvOsSemaphoreHandle hSema)
{
    NvBool ret;

    HOST_CLOCK_ENABLE( hDevice );

    ret = NvRmChPrivSyncPointSignalSemaphore( hDevice, SyncPointID, Threshold,
        hSema);
    if( !ret )
    {
        NvOsSemaphoreWait( hSema );
    }

    HOST_CLOCK_DISABLE( hDevice );
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
    NvBool ret;
    NvError err;
    NvOsSemaphoreHandle sema;

    err = NvOsSemaphoreCreate(&sema, 0);
    if (err != NvSuccess)
        return err;

    HOST_CLOCK_ENABLE( hDevice );

    ret = NvRmChPrivSyncPointSignalSemaphore( hDevice, Fence->SyncPointID,
        Fence->Value, sema);
    if(!ret)
    {
        err = NvOsSemaphoreWaitTimeout( sema, Timeout );
    }

    NvOsSemaphoreDestroy(sema);

    HOST_CLOCK_DISABLE( hDevice );

    if( err != NvSuccess )
    {
        return NvError_Timeout;
    }

    return NvSuccess;
}

NvError
NvRmChannelSyncPointWaitTimeout( NvRmDeviceHandle hDevice,
    NvU32 SyncPointID, NvU32 Threshold, NvOsSemaphoreHandle hSema,
    NvU32 Timeout )
{
    return NvRmChannelSyncPointWaitexTimeout( hDevice, SyncPointID, Threshold,
        hSema, Timeout, NULL );
}

NvError
NvRmChannelSyncPointWaitexTimeout( NvRmDeviceHandle hDevice,
    NvU32 SyncPointID, NvU32 Threshold, NvOsSemaphoreHandle hSema,
    NvU32 Timeout, NvU32 *Actual )
{
    NvError e = NvSuccess;
    NvBool ret;

    HOST_CLOCK_ENABLE( hDevice );

    ret = NvRmChPrivSyncPointSignalSemaphore( hDevice, SyncPointID, Threshold,
        hSema);
    if( !ret )
    {
        e = NvOsSemaphoreWaitTimeout( hSema, Timeout );
    }

    HOST_CLOCK_DISABLE( hDevice );

    if( e != NvSuccess )
    {
        return NvError_Timeout;
    }

    if( Actual )
    {
        *Actual = Threshold;
    }
    return NvSuccess;
}

NvError
NvRmFenceGetFromFile( NvS32 Fd, NvRmFence *Fences, NvU32 *NumFences )
{
    NV_ASSERT(!"NvRmFenceGetFromFile not supported");
    return NvError_NotSupported;
}

NvError
NvRmFencePutToFile( const char *Name, const NvRmFence *Fences,
    NvU32 NumFences, NvS32 *Fd )
{
    NV_ASSERT(!"NvRmFencePutToFile not supported");
    return NvError_NotSupported;
}

NvU32
NvRmChannelGetModuleWaitBase( NvRmChannelHandle hChannel, NvRmModuleID Module,
    NvU32 Index )
{
    return 0;
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

NvError
NvRmChannelSetSubmitTimeoutEx(
    NvRmChannelHandle ch,
    NvU32 SubmitTimeout,
    NvBool DisableDebugDump)
{
    return NvError_NotImplemented;
}

NvError
NvRmChannelSetSubmitTimeout(
    NvRmChannelHandle ch,
    NvU32 SubmitTimeout)
{
    return NvError_NotImplemented;
}

NvS32
NvRmChannelGetModuleTimedout(
    NvRmChannelHandle hChannel,
    NvRmModuleID Module,
    NvU32 Index)
{
    return -1;
}

NvError
NvRmPrivIncrSyncPointShadow( NvRmDeviceHandle hDevice, NvU32 id, NvU32 incr,
    NvU32 *oldValue, NvU32 *value )
{
    NV_ASSERT( hDevice );
    NV_ASSERT( id < NV_HOST1X_SYNCPT_NB_PTS );

    NvOsMutexLock( g_channelMutex );

    if (oldValue)
        *oldValue = g_channelState.SyncPointShadow.syncpt[id];
    g_channelState.SyncPointShadow.syncpt[id] += incr;
    *value = g_channelState.SyncPointShadow.syncpt[id];
    NVRM_RMC_TRACE((s_rmc, "# Sync point %d is now %d\n\n",
        id, g_channelState.SyncPointShadow.syncpt[id] ));

    NvOsMutexUnlock( g_channelMutex );

    return NvSuccess;
}

NvError
NvRmChannelGetModuleSyncPoint( NvRmChannelHandle hChannel,
    NvRmModuleID Module,
    NvU32 SyncPointIndex,
    NvU32 *pSyncPointID )
{
    NvU32 mod;

    mod = NVRM_MODULE_ID_MODULE( Module );

    NV_ASSERT( hChannel );

    switch( mod ) {
    case NvRmModuleID_Display:
        if (SyncPointIndex == 0)
        {
            if( NVRM_MODULE_ID_INSTANCE( Module) == 0 )
            {
                *pSyncPointID = NVRM_SYNCPOINT_DISP0;
            }
            else
            {
                *pSyncPointID = NVRM_SYNCPOINT_DISP1;
            }
        }
        else if (SyncPointIndex == 1)
        {
            if( NVRM_MODULE_ID_INSTANCE( Module) == 0 )
            {
                *pSyncPointID = NVRM_SYNCPOINT_VBLANK0;
            }
            else
            {
                *pSyncPointID = NVRM_SYNCPOINT_VBLANK1;
            }
        }
        break;

    case NvRmModuleID_Dsi:
        *pSyncPointID = NVRM_SYNCPOINT_DSI;
        break;

    default:
        NV_ASSERT(!"Unknown module id passed to "
            "NvRmChannelGetModuleSyncPoint");
        return NvSuccess;
    }

    /* do not reset the sync point */

    return NvSuccess;
}

NvU32
NvRmChannelGetModuleMutex(
    NvRmModuleID Module,
    NvU32 Index)
{
    NvU32 mod;

    mod = NVRM_MODULE_ID_MODULE( Module );

    switch( mod ) {
    case NvRmModuleID_Display:
        NV_ASSERT( Index == 0 || Index == 1 );
        if( Index == 0 )
        {
            return NVRM_MODULEMUTEX_DISPLAYA;
        }
        else
        {
            return NVRM_MODULEMUTEX_DISPLAYB;
        }

    case NvRmModuleID_Dsi:
        return NVRM_MODULEMUTEX_DSI;
    default:
        return 0;
    }
}

void
NvRmChannelModuleMutexLock( NvRmDeviceHandle hDevice, NvU32 Index )
{
    NvU32 reg;
#if NV_DEBUG
    NvU32 tmp;
#endif

    HOST_CLOCK_ENABLE( hDevice );

    /* mlock registers returns 0 when the lock is aquired. writing 0 clears
     * the lock.
     */
    do
    {
        NV_DEBUG_CODE(
            tmp = HOST1X_SYNC_REGR( HOST1X_SYNC_MLOCK_OWNER_0_0 +
                (Index * 4) );
            if( NV_DRF_VAL( HOST1X_SYNC, MLOCK_OWNER_0, MLOCK_CPU_OWNS_0,
                    tmp ) )
            {
                NvOsDebugPrintf( "cpu owns mlock: %d\n", Index );
                NV_ASSERT( !"cpu owns" );
            }
            if( NV_DRF_VAL( HOST1X_SYNC, MLOCK_OWNER_0, MLOCK_CH_OWNS_0,
                    tmp ) )
            {
                /**
                 * uncomment this to see what's going wrong.
                 *
                 * NvU32 ch;
                 * ch = NV_DRF_VAL( HOST1X_SYNC, MLOCK_OWNER_0,
                 *         MLOCK_OWNER_CHID_0, tmp );
                 * NvOsDebugPrintf( "channel %d owns mlock: %d\n", ch, Index );
                 */
            }
        );

        reg = HOST1X_SYNC_REGR( HOST1X_SYNC_MLOCK_0_0 + (Index * 4) );
    } while( reg );

    HOST_CLOCK_DISABLE( hDevice );
}

void
NvRmChannelModuleMutexUnlock( NvRmDeviceHandle hDevice, NvU32 Index )
{
    HOST_CLOCK_ENABLE( hDevice );
    HOST1X_SYNC_REGW( HOST1X_SYNC_MLOCK_0_0 + (Index * 4), 0 );
    HOST_CLOCK_DISABLE( hDevice );
}

void
NvRmChModuleRegRd(NvRmDeviceHandle device, NvRmModuleID id, NvU32 num,
    const NvU32 *offsets, NvU32 *values)
{
    NvU32 mod;
    NvU32 index;
    volatile NvU32 *HostChRegs;
    NvU32 FirstOffset;
    NvU32 NumRegToRead;
    NvU32 FifoStat;
    NvU32 ReadValueReady;
    NvU32 ModuleId;

    mod = NVRM_MODULE_ID_MODULE( id );

    HostChRegs = (NvU32*)((NvUPtr)g_channelState.Host1xBaseVirtual
            + NV_HOST1X_CHANNEL_PROT_BASE);

    switch( mod ) {
    case NvRmModuleID_Display:
        if( NVRM_MODULE_ID_INSTANCE( id ) == 0 )
        {
            ModuleId = NV_HOST1X_MODULEID_DISPLAY;
        }
        else
        {
            ModuleId = NV_HOST1X_MODULEID_DISPLAYB;
        }
        break;
    case NvRmModuleID_Dsi:
        ModuleId = NV_HOST1X_MODULEID_DSI;
        break;

    default:
        NV_ASSERT( !"bad module id for NvRmChModuleRd" );
        return;
    }

    NvOsMutexLock(g_ModuleRegRdWrLock);
    index = 0;
    while (index < num)
    {
        FirstOffset  = (offsets[index] >> 2);
        NumRegToRead = 1;

        // This loop will try to find contiguous chunks of registers to read,
        // so that we can issue a single read to get a bunch of registers
        // instead of one register at a time.
        //
        // First part of the condition checks that we are not reading more than
        // thre requested 'num' of registers to be read.
        // Second part limits the "burst" read to just 8 registers.
        while ( (index + NumRegToRead) < num && NumRegToRead < 8)
        {
            if( (FirstOffset + NumRegToRead) ==
                (offsets[index + NumRegToRead] >> 2) )
            {
                ++NumRegToRead;
            }
            else
            {
                break;
            }
        }

        // now do the actual reading.
        CHANNEL_IND_REGW(HostChRegs, INDOFF,  // set the address to read
            NV_DRF_DEF(HOST1X, CHANNEL_INDOFF, AUTOINC, ENABLE) |
            NV_DRF_DEF(HOST1X, CHANNEL_INDOFF, ACCTYPE, REG) |
            NV_DRF_NUM(HOST1X, CHANNEL_INDOFF, INDMODID, ModuleId) |
            NV_DRF_NUM(HOST1X, CHANNEL_INDOFF, INDROFFSET, FirstOffset));

        CHANNEL_IND_REGW(HostChRegs, INDCNT, // set the count
           NV_DRF_NUM(HOST1X, CHANNEL_INDCNT, INDCOUNT, NumRegToRead));

        while ( NumRegToRead )
        {
            // read the data
            FifoStat = CHANNEL_IND_REGR(HostChRegs, FIFOSTAT);
            // extract HOST1X_CHANNEL_FIFOSTAT_0_OUTFENTRIES_SHIFT from
            // Fifo State

            ReadValueReady = NV_DRF_VAL(HOST1X, CHANNEL_FIFOSTAT,
                OUTFENTRIES, FifoStat);
            while ( ReadValueReady )
            {
                values[index] = CHANNEL_IND_REGR(HostChRegs, INDDATA);
                ++index;
                --NumRegToRead;
                --ReadValueReady;
            }
        }
    }
    NvOsMutexUnlock(g_ModuleRegRdWrLock);
}

void
NvRmChModuleRegWr( NvRmDeviceHandle device, NvRmModuleID id, NvU32 num,
    const NvU32 *offsets, const NvU32 *values )
{
    NvU32 mod;
    NvU32 index;
    volatile NvU32 *HostChRegs;
    NvU32 FifoStat = 0;
    NvU32 FreeWriteEntries = 0;
    NvU32 PreviousOffsetWritten;
    NvU32 ModuleId;

    mod = NVRM_MODULE_ID_MODULE( id );
    HostChRegs = (NvU32*)((NvUPtr)g_channelState.Host1xBaseVirtual +
        NV_HOST1X_CHANNEL_PROT_BASE);

    // pick a value that will force indoff to update first pass
    PreviousOffsetWritten = 0xFFFFFFFE;

    switch( mod ) {
    case NvRmModuleID_Display:
        if( NVRM_MODULE_ID_INSTANCE( id ) == 0 )
        {
            ModuleId = NV_HOST1X_MODULEID_DISPLAY;
        }
        else
        {
            ModuleId = NV_HOST1X_MODULEID_DISPLAYB;
        }
        break;
    case NvRmModuleID_Dsi:
        ModuleId = NV_HOST1X_MODULEID_DSI;
        break;

    default:
        NV_ASSERT( !"bad module id for NvRmChModuleWr" );
        return;
    }

    NvOsMutexLock(g_ModuleRegRdWrLock);
    for (index=0; index < num; ++index)
    {
        // Read the fifo stat and make sure there is room for more entries
        while (FreeWriteEntries == 0)
        {
            FifoStat = CHANNEL_IND_REGR(HostChRegs, FIFOSTAT);
            FreeWriteEntries = NV_DRF_VAL(HOST1X, CHANNEL_FIFOSTAT,
                REGFNUMEMPTY, FifoStat);
        }

        if ( (PreviousOffsetWritten + 1) != (offsets[index] >> 2) )
        {
            // Setup the address to write
            CHANNEL_IND_REGW(HostChRegs, INDOFF, // set the address to read
                NV_DRF_DEF(HOST1X, CHANNEL_INDOFF, AUTOINC, ENABLE) |
                NV_DRF_DEF(HOST1X, CHANNEL_INDOFF, ACCTYPE, REG) |
                NV_DRF_NUM(HOST1X, CHANNEL_INDOFF, INDMODID, ModuleId) |
                NV_DRF_NUM(HOST1X, CHANNEL_INDOFF, INDROFFSET,
                    (offsets[index] >> 2) ));
        }

        // write the data
        CHANNEL_IND_REGW(HostChRegs, INDDATA, values[index]);
        --FreeWriteEntries;
        PreviousOffsetWritten = (offsets[index] >> 2);
    }
    NvOsMutexUnlock(g_ModuleRegRdWrLock);
}

// save host1x state and shut it down.
NvError
NvRmPowerSuspend( NvRmDeviceHandle hDevice )
{
    ACM_PRINTF(("===== HOST BEGIN SUSPEND\n"));
    NV_ASSERT(CHANNEL_IS_INITIALIZED());

    NvOsMutexLock( s_Acm.Mutex );

    NV_ASSERT(!s_Acm.HostIsSuspended);
    s_Acm.HostIsSuspended = NV_TRUE;

    NvRmChPrivPowerdown( hDevice, NV_TRUE, NULL);
    NvRmChPrivDisableHost( hDevice );

    NvOsMutexUnlock( s_Acm.Mutex );

    ACM_PRINTF(("===== HOST SUSPEND COMPLETE\n"));
    return NvSuccess;
}

// restore host1x state after suspend.
NvError
NvRmPowerResume( NvRmDeviceHandle hDevice )
{
    ACM_PRINTF(("===== HOST BEGIN RESUME\n"));
    NV_ASSERT(CHANNEL_IS_INITIALIZED());

    NvOsMutexLock( s_Acm.Mutex );

    NV_ASSERT(s_Acm.HostIsSuspended);
    s_Acm.HostIsSuspended = NV_FALSE;

    NV_ASSERT_SUCCESS(
        NvRmChPrivEnableHost( hDevice ) );

    NvOsMutexUnlock( s_Acm.Mutex );
    ACM_PRINTF(("===== HOST RESUME COMPLETE\n"));
    return NvSuccess;
}

/* Turn on power to the host1x and restore its state.
 *
 * NOTE: this assumes that s_Acm.Mutex is held!
 */
static NvError
NvRmChPrivEnableHost( NvRmDeviceHandle hDevice )
{
    // 1. Re-enable Graphics Host clock
    // 2. Re-init host and reset channel
    // 3. Restore states.  Sync points stuff for now

    NvError err;
    NvRmPowerEvent event;
    NvU8    i;

    NV_ASSERT(CHANNEL_IS_INITIALIZED());
    NV_ASSERT(!s_Acm.HostRegistersAreValid);

    ACM_PRINTF(("===== HOST POWER ON\n"));
    // This can only fail the first time it is called (allocates memory)
    err = NvRmChPrivHostVoltageEnable(hDevice);
    if (err)
        return err;
    s_Acm.HostRegistersAreValid = NV_TRUE;

    HOST_CLOCK_ENABLE( hDevice );

    NV_ASSERT_SUCCESS(
        NvRmPowerGetEvent( hDevice, g_HostPowerClientID, &event )
    );

    if( event == NvRmPowerEvent_WakeLP0 ||
        event == NvRmPowerEvent_WakeLP1 )
    {
        // Only restore if hardware states are lost to LP0/LP1
        for( i = 0; i < g_channelState.NumChannels; i++ )
        {
            if ( (g_channelState.Channels[i].HwState)->refcount )
            {
                NvRmChHw *hw = &g_channelState.ChHwState[i];
                /* set base, end pointer (all of memory) */
                CHANNEL_REGW( hw, DMASTART, 0 );
                CHANNEL_REGW( hw, DMAEND, 0xFFFFFFFF );
                NvRmPrivChannelReset( &g_channelState.Channels[i] );
            }
        }

        NvRmChPrivRestoreSyncPoints( hDevice );
        NvRmChPrivRestoreMlocks( hDevice );
    }
    HOST_CLOCK_DISABLE( hDevice );
    return NvSuccess;
}

/* Prepare to turn off power to host1x.
 *
 * Currently this is only called when suspending.
 *
 * NOTE: this assumes that s_Acm.Mutex is held!
 */
static void
NvRmChPrivDisableHost( NvRmDeviceHandle hDevice )
{
    // 1. Check if any pending sync point semaphore
    // 2. Save states.  Sync points and mlocks.
    // 3. Disable Graphics Host clock

    HOST_CLOCK_ENABLE( hDevice );

    // TODO: Is it possible g_SyncPointWaitListCount could be nonzero when we
    // go into suspend?  (If so we will hit this assert.)  If so we need to
    // save and restore g_SyncPointWaitListCountHoldsHostClockRef and
    // g_SyncPointWaitListCount so that NvRmHostInterruptService() (in
    // ap15rm_hostintr.c) does not read host registers while the host clock is
    // off.  We also need to call HOST_CLOCK_DISABLE if
    // g_SyncPointWaitListCountHoldsHostClockRef was set on suspend and
    // corresponding HOST_CLOCK_DISABLE on resume.
    NV_ASSERT(!g_SyncPointWaitListCountHoldsHostClockRef);

    NvRmChPrivSaveSyncPoints( hDevice );
    NvRmChPrivSaveMlocks( hDevice );

    NV_ASSERT(s_Acm.HostClockRef == 1);

    HOST_CLOCK_DISABLE( hDevice );

    ACM_PRINTF(("===== HOST POWER OFF\n"));

    NV_ASSERT(s_Acm.HostRegistersAreValid);
    s_Acm.HostRegistersAreValid = NV_FALSE;
    NvRmChPrivHostVoltageDisable( hDevice );
}

void
NvRmChPrivSaveSyncPoints( NvRmDeviceHandle hDevice )
{
    NvU32   idx;
    NvU32   ChannelClassReg;

    /* always save/restore all sync points. it's too hard to know if a module
     * isn't using them.
     */
    for (idx = 0; idx < NV_HOST1X_SYNCPT_NB_PTS; idx++)
    {
        s_SyncPointStates.syncpt[idx] =
            HOST1X_SYNC_REGR( HOST1X_SYNC_SYNCPT_0_0 + (idx * 4) );
        s_SyncPointStates.thresh[idx] =
            HOST1X_SYNC_REGR( HOST1X_SYNC_SYNCPT_INT_THRESH_0_0 +
            (idx * 4) );
    }

    for (idx = 0; idx < NV_HOST1X_SYNCPT_NB_BASES; idx++)
    {
        s_SyncPointStates.base[idx] =
            HOST1X_SYNC_REGR( HOST1X_SYNC_SYNCPT_BASE_0_0 +
            (idx * 4) );
    }

    for (idx = 0; idx < NV_HOST1X_CHANNELS; idx++)
    {
        ChannelClassReg =
            HOST1X_SYNC_REGR( HOST1X_SYNC_CBSTAT0_0 + (idx * 4) );
        s_SyncPointStates.ClassId[idx] =
            NV_DRF_VAL(HOST1X_SYNC, CBSTAT0, CBCLASS0, ChannelClassReg);
    }
}

void
NvRmChPrivRestoreSyncPoints( NvRmDeviceHandle hDevice )
{
    NvU32               idx;

    for (idx = 0; idx < NV_HOST1X_SYNCPT_NB_PTS; idx++)
    {
        HOST1X_SYNC_REGW( HOST1X_SYNC_SYNCPT_0_0 + (idx * 4),
            s_SyncPointStates.syncpt[idx] );
        HOST1X_SYNC_REGW( HOST1X_SYNC_SYNCPT_INT_THRESH_0_0 +
            (idx * 4), s_SyncPointStates.thresh[idx] );
    }

    for (idx = 0; idx < NV_HOST1X_SYNCPT_NB_BASES; idx++)
    {
        HOST1X_SYNC_REGW( HOST1X_SYNC_SYNCPT_BASE_0_0 + (idx * 4),
           s_SyncPointStates.base[idx] );
    }

    return;
}

/* just store the CPU owner state. channel submits should have a mlock
 * release opcode. the host should not be powered down until submits have
 * finished.
 */
void NvRmChPrivSaveMlocks( NvRmDeviceHandle hDevice )
{
    NvU32 i;
    NvU32 reg;

    for( i = 0; i < NV_HOST1X_NB_MLOCKS; i++ )
    {
        reg = HOST1X_SYNC_REGR( HOST1X_SYNC_MLOCK_OWNER_0_0 + (i * 4) );
        s_MlockState[i] = NV_DRF_VAL( HOST1X_SYNC, MLOCK_OWNER_0,
            MLOCK_CPU_OWNS_0, reg );
    }
}

static void
NvRmChPrivRestoreMlocks( NvRmDeviceHandle hDevice )
{
    NvU32 i;

    for( i = 0; i < NV_HOST1X_NB_MLOCKS; i++ )
    {
        if( s_MlockState[i] )
        {
            /* force-aquire the mlock */
            NvRmChannelModuleMutexLock( hDevice, i );
        }
        else
        {
            NvRmChannelModuleMutexUnlock( hDevice, i );
        }
    }
}

NvError
NvRmHostModuleRegRd( NvRmDeviceHandle device, NvRmModuleID id, NvU32 num,
    const NvU32 *offsets, NvU32 *values )
{
    NvU32 mod;
    NvU32 instance;

    mod = NVRM_MODULE_ID_MODULE( id );

    NV_ASSERT( mod == NvRmModuleID_Display  ||
        mod == NvRmModuleID_Dsi );

    instance = NVRM_MODULE_ID_INSTANCE( id );

    HOST_CLOCK_ENABLE( device );

    NV_REGR_MULT( device, NVRM_MODULE_ID( mod, instance), num, offsets,
        values );

    HOST_CLOCK_DISABLE( device );

    return NvSuccess;
}

NvError
NvRmHostModuleRegWr( NvRmDeviceHandle device, NvRmModuleID id, NvU32 num,
    const NvU32 *offsets, const NvU32 *values )
{
    NvU32 mod;
    NvU32 instance;

    mod = NVRM_MODULE_ID_MODULE( id );

    NV_ASSERT( mod == NvRmModuleID_Display  ||
        mod == NvRmModuleID_Dsi );

    instance = NVRM_MODULE_ID_INSTANCE( id );

    HOST_CLOCK_ENABLE( device );

    NV_REGW_MULT( device, NVRM_MODULE_ID( mod, instance ), num, offsets,
        values );

    HOST_CLOCK_DISABLE( device );

    return NvSuccess;
}

NvError
NvRmHostModuleBlockRegWr( NvRmDeviceHandle device, NvRmModuleID id, NvU32 num,
    NvU32 offset, const NvU32 *values )
{
    NvU32 mod;
    NvU32 instance;

    mod = NVRM_MODULE_ID_MODULE( id );

    NV_ASSERT( mod == NvRmModuleID_Display  ||
        mod == NvRmModuleID_Dsi );

    instance = NVRM_MODULE_ID_INSTANCE( id );

    HOST_CLOCK_ENABLE( device );

    NV_REGW_BLOCK( device, NVRM_MODULE_ID( mod, instance), num, offset,
        values );

    HOST_CLOCK_DISABLE( device );

    return NvSuccess;
}

NvError
NvRmHostModuleBlockRegRd( NvRmDeviceHandle device, NvRmModuleID id, NvU32 num,
    NvU32 offset, NvU32 *values )
{
    NvU32 mod;
    NvU32 instance;

    mod = NVRM_MODULE_ID_MODULE( id );

    NV_ASSERT( mod == NvRmModuleID_Display  ||
        mod == NvRmModuleID_Dsi );

    instance = NVRM_MODULE_ID_INSTANCE( id );

    HOST_CLOCK_ENABLE( device );

    NV_REGR_BLOCK( device, NVRM_MODULE_ID( mod, instance ), num, offset,
        values );

    HOST_CLOCK_DISABLE( device );

    return NvSuccess;
}

void
NvRmChPrivIncrSyncpt(NvU32 SyncPointID)
{
    NvU32 reg;
    NvU32 idx, offset;
    idx    = (SyncPointID>>5);
    offset = (SyncPointID & 0x1F);

#ifdef HOST1X_SYNC_SYNCPT_CPU_INCR_0_0
    reg = NV_DRF_NUM( HOST1X_SYNC, SYNCPT_CPU_INCR_0,
            SYNCPT_CPU_INCR_VECTOR_0, (1 << offset) );

    HOST1X_SYNC_REGW( HOST1X_SYNC_SYNCPT_CPU_INCR_0_0 + idx, reg );
#else
    reg = NV_DRF_NUM( HOST1X_SYNC, SYNCPT_CPU_INCR,
            SYNCPT_CPU_INCR_VECTOR, (1 << offset) );

    HOST1X_SYNC_REGW( HOST1X_SYNC_SYNCPT_CPU_INCR_0 + idx, reg );
#endif
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
    return NvRmHostModuleRegRd(device,
        id, num, offsets, values);
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
    return NvRmHostModuleRegWr(device,
        id, num, offsets, values);
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
    return NvRmHostModuleBlockRegRd(device,
        id, num, offset, values);
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
    return NvRmHostModuleBlockRegWr(device,
        id, num, offset, values);
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
