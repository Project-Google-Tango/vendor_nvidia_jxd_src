/*
 * Copyright (c) 2007-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#include "nvcommon.h"
#include "nvassert.h"
#include "nvfxmath.h"
#include "nvrm_analog.h"
#include "nvrm_module.h"
#include "nvrm_memmgr.h"
#include "nvrm_interrupt.h"
#include "nvrm_pinmux.h"
#include "nvrm_pmu.h"
#include "nvddk_disp_hw.h"
#include "dc_hal.h"
#include "dc_hdmi_hal.h"
#include "dc_dsi_hal.h"
#include "disp_table.h"
#include "nvrm_hardware_access.h"

/* Note on hw spec: this always uses the Window B spec (ardispay_b.h)
 * since it is the most fully-featured (all the defines are there),
 * and every window has identical register layouts.
 */

// FIXME: check for window caps

DcController s_Controllers[DC_HAL_CONTROLLERS];

DcPwmOutputPinInfo PwmOutputPinTable[] =
{
    {(NvU32)('w'-'a'), 1, DcPwmOutputPin_LM1_PM1},
    {(NvU32)('n'-'a'), 6, DcPwmOutputPin_LDC_PM1},
    {(NvU32)('b'-'a'), 2, DcPwmOutputPin_LPW0_PM0},
    {(NvU32)('c'-'a'), 1, DcPwmOutputPin_LPW1_PM1},
    {(NvU32)('c'-'a'), 6, DcPwmOutputPin_LPW2_PM0},
    {(NvU32)('w'-'a'), 0, DcPwmOutputPin_LM0_PM0},
};

NvBool
DcHal_HwOpen( NvRmDeviceHandle hRm, NvU32 controller,
    NvDdkDispCapabilities *caps, NvBool bReopen )
{
    NvError e;
    DcController *cont;
    NvRmModuleID module[1];
    NvU32 i;

    NV_ASSERT(controller < DC_HAL_MAX_CONTROLLERS);
    NV_ASSERT(hRm);

    cont = &s_Controllers[ controller ];
    if( cont->bInitialized )
    {
        if( !cont->bClocked )
        {
            DcResume( cont );
        }
        return NV_TRUE;
    }

    NvOsMemset( cont, 0, sizeof(DcController) );

    cont->hRm = hRm;
    cont->caps = caps;
    cont->index = controller;
    cont->SyncPointId = NVRM_INVALID_SYNCPOINT_ID;
    cont->VsyncSyncPointId = NVRM_INVALID_SYNCPOINT_ID;
    cont->reg = &cont->regs[0];
    cont->val = &cont->vals[0];
    cont->fence = cont->regs + DC_HAL_REG_BATCH_SIZE;
    cont->pb = 0;
    cont->stream_fw = NULL;
    cont->base = NvOdmDispBaseColorSize_666;
    cont->align = NvOdmDispDataAlignment_MSB;
    cont->pinmap = NvOdmDispPinMap_Single_Rgb18;
    cont->PwmOutputPin = DcPwmOutputPin_Unspecified;
    cont->InterruptHandle = NULL;
    cont->bReopen = bReopen;

    NV_CHECK_ERROR_CLEANUP(
        NvOsIntrMutexCreate( &cont->IntrMutex )
    );

    for( i = 0; i < DC_HAL_WINDOWS; i++ )
    {
        NvOsMemset( &cont->windows[i], 0, sizeof(cont->windows[i]) );

        cont->windows[i].index = i;
        cont->windows[i].blend_op = NvDdkDispBlendType_None;
        cont->windows[i].alpha_op = NvDdkDispAlphaOperation_WeightedMean;
        cont->windows[i].alpha = 255;
        cont->windows[i].depth = (NvU32)-1;
    }

    /* map the registers */
    NvRmModuleGetBaseAddress( hRm,
        NVRM_MODULE_ID( NvRmModuleID_Display, cont->index ),
        &cont->aperture_phys, &cont->aperture_size );

    NV_CHECK_ERROR_CLEANUP(
        NvRmPhysicalMemMap( cont->aperture_phys, cont->aperture_size,
            NVOS_MEM_READ_WRITE, NvOsMemAttribute_Uncached, &cont->registers )
    );

    cont->PowerClientId = NVRM_POWER_CLIENT_TAG('D','C','*','*');
    NV_CHECK_ERROR_CLEANUP(
        NvRmPowerRegister( hRm, 0, &cont->PowerClientId )
    );

    /* allocate a semaphore */
    NV_CHECK_ERROR_CLEANUP(
        NvOsSemaphoreCreate(&cont->sem, 0)
    );

    /* get a channel (need one for the sync point api) */
    module[0] = NVRM_MODULE_ID( NvRmModuleID_Display, cont->index );
    NV_CHECK_ERROR_CLEANUP(
        NvRmChannelOpen( hRm, &cont->ch, 1, module )
    );

    /* get a sync point */
    NV_CHECK_ERROR_CLEANUP(
        NvRmChannelGetModuleSyncPoint( cont->ch,
            NVRM_MODULE_ID( NvRmModuleID_Display, cont->index ),
            0, &cont->SyncPointId )
    );

    /* the sync point may have been used before by a previous open/close
     * of display -- get it's current value
     */
    cont->SyncPointShadow = NvRmChannelSyncPointRead( cont->hRm,
        cont->SyncPointId );

    /* get the vsync sync point */
    NV_CHECK_ERROR_CLEANUP(
        NvRmChannelGetModuleSyncPoint( cont->ch,
            NVRM_MODULE_ID( NvRmModuleID_Display, cont->index ),
            1, &cont->VsyncSyncPointId )
    );

    /* get the hardware mutex */
    cont->hw_mutex = NvRmChannelGetModuleMutex( NvRmModuleID_Display,
        cont->index );

    /* need to coordinate frame updates with other uses of the display bus
     * (DSI in particular)
     */
    NV_CHECK_ERROR_CLEANUP(
        NvOsMutexCreate( &cont->frame_mutex )
    );

    /* Smartdimmer statistics coordination flag. */
    NV_CHECK_ERROR_CLEANUP(
        NvOsMutexCreate( &cont->sdMutex )
    );

    /* setup the stream, just in case */
    NV_CHECK_ERROR_CLEANUP(
        NvRmStreamInit( hRm, cont->ch, &cont->stream )
    );

    /* enable the hardware */
    if( bReopen )
    {
        DcSetClockPower( cont->hRm, cont->PowerClientId, cont->index, NV_TRUE,
            NV_TRUE );
        cont->bClocked = NV_TRUE;
    }
    else
    {
        DcResume( cont );
    }

    cont->bInitialized = NV_TRUE;
    return NV_TRUE;

fail:
    DcHal_HwClose( controller );
    return NV_FALSE;
}

/**
 * This will block until a frame has finished. This has its own stream
 * management.
 */
void
DcWaitFrame( DcController *cont )
{
    NvU32 val;

    DC_BEGIN( cont );

    HOST_CLOCK_ENABLE( cont );

    /* wait for a new frame to finish */
    cont->SyncPointShadow += 2;

    if( NVDDK_DISP_CAP_IS_SET( cont->caps,
            NvDdkDispCapBit_Bug_Double_Syncpt_Incr ) )
    {
        DcLatch( cont, NV_FALSE );

        val = NV_DRF_DEF( DC_CMD, GENERAL_INCR_SYNCPT, GENERAL_COND,
                REG_WR_SAFE )
            | NV_DRF_NUM( DC_CMD, GENERAL_INCR_SYNCPT, GENERAL_INDX,
                cont->SyncPointId );
        DC_WRITE( cont, DC_CMD_GENERAL_INCR_SYNCPT_0, val );
        DC_WRITE( cont, DC_CMD_GENERAL_INCR_SYNCPT_0, val );
    }
    else
    {
        val = NV_DRF_DEF( DC_CMD, GENERAL_INCR_SYNCPT, GENERAL_COND,
                FRAME_START )
            | NV_DRF_NUM( DC_CMD, GENERAL_INCR_SYNCPT, GENERAL_INDX,
                cont->SyncPointId );
        DC_WRITE( cont, DC_CMD_GENERAL_INCR_SYNCPT_0, val );

        val = NV_DRF_DEF( DC_CMD, GENERAL_INCR_SYNCPT, GENERAL_COND,
                FRAME_DONE )
            | NV_DRF_NUM( DC_CMD, GENERAL_INCR_SYNCPT, GENERAL_INDX,
                cont->SyncPointId );
        DC_WRITE( cont, DC_CMD_GENERAL_INCR_SYNCPT_0, val );
    }

    DcLatch( cont, NV_FALSE );
    DC_END( cont );
    DC_FLUSH( cont );

    NvRmChannelSyncPointWait( cont->hRm, cont->SyncPointId,
        cont->SyncPointShadow, cont->sem );

    HOST_CLOCK_DISABLE( cont );
}

void
DcPushWaitFrame( DcController *cont )
{
    NvRmStream *stream;
    NvU32 val;

    stream = (cont->stream_fw) ? cont->stream_fw : &cont->stream;

    /* WARNING: there's a rather nasty hardware bug that results in a
     * double-increment of the sync point. see bug 580685. the workaround
     * is to use two back-to-back reg-wr-safe increments instead of
     * frame_done. also need to set the no-stall bit in sync point control
     * register (done at init time).
     */

    if( cont->pb && NVDDK_DISP_CAP_IS_SET( cont->caps,
            NvDdkDispCapBit_Bug_Double_Syncpt_Incr ) )
    {
        /* wait for the current frame to finish */
        cont->SyncPointShadow += 2;

        val = NV_DRF_DEF( DC_CMD, GENERAL_INCR_SYNCPT, GENERAL_COND,
                REG_WR_SAFE )
            | NV_DRF_NUM( DC_CMD, GENERAL_INCR_SYNCPT, GENERAL_INDX,
                cont->SyncPointId );
        DC_WRITE_STREAM( cont, DC_CMD_GENERAL_INCR_SYNCPT_0, val, cont->pb );
        DC_WRITE_STREAM( cont, DC_CMD_GENERAL_INCR_SYNCPT_0, val, cont->pb );

        NVRM_STREAM_PUSH_U( cont->pb,
            NVRM_CH_OPCODE_SET_CLASS( NV_HOST1X_CLASS_ID, 0, 0 ) );

        /* applying vsync wait only for non-TE mode, since it will cause
         * the frame rate to be limited to half of the refresh rate
         */
        if ( !( cont->mode.flags & NVDDK_DISP_MODE_FLAG_USE_TEARING_EFFECT ) )
        {
            /* make sure we've actually waited a frame */
            val = NV_DRF_NUM( NV_CLASS_HOST, WAIT_SYNCPT_INCR, INDX,
                cont->VsyncSyncPointId );
            NVRM_STREAM_PUSH_U(cont->pb,
                NVRM_CH_OPCODE_NONINCR( NV_CLASS_HOST_WAIT_SYNCPT_INCR_0, 1 )
                );
            NVRM_STREAM_PUSH_U( cont->pb, val );
        }

        val = NV_DRF_NUM( NV_CLASS_HOST, WAIT_SYNCPT, INDX,
                cont->SyncPointId )
            | NV_DRF_NUM( NV_CLASS_HOST, WAIT_SYNCPT, THRESH,
                cont->SyncPointShadow );

        NVRM_STREAM_PUSH_U( cont->pb,
            NVRM_CH_OPCODE_NONINCR( NV_CLASS_HOST_WAIT_SYNCPT_0, 1 ) );
        NVRM_STREAM_PUSH_U( cont->pb, val );

        NVRM_STREAM_PUSH_WAIT_CHECK( stream, cont->pb, cont->SyncPointId,
            cont->SyncPointShadow );

        /* set-class back to display */
        DcPushSetcl( cont );
    }
    else if( cont->pb )
    {
        /* wait for the current frame to finish */
        cont->SyncPointShadow++;

        val = NV_DRF_DEF( DC_CMD, GENERAL_INCR_SYNCPT, GENERAL_COND,
                FRAME_DONE )
            | NV_DRF_NUM( DC_CMD, GENERAL_INCR_SYNCPT, GENERAL_INDX,
                cont->SyncPointId );
        DC_WRITE_STREAM( cont, DC_CMD_GENERAL_INCR_SYNCPT_0, val, cont->pb );

        NVRM_STREAM_PUSH_U( cont->pb,
            NVRM_CH_OPCODE_SET_CLASS( NV_HOST1X_CLASS_ID, 0, 0 ) );

        val = NV_DRF_NUM( NV_CLASS_HOST, WAIT_SYNCPT, INDX,
                cont->SyncPointId )
            | NV_DRF_NUM( NV_CLASS_HOST, WAIT_SYNCPT, THRESH,
                cont->SyncPointShadow );

        NVRM_STREAM_PUSH_U( cont->pb,
            NVRM_CH_OPCODE_NONINCR( NV_CLASS_HOST_WAIT_SYNCPT_0, 1 ) );
        NVRM_STREAM_PUSH_U( cont->pb, val );

        NVRM_STREAM_PUSH_WAIT_CHECK( stream, cont->pb, cont->SyncPointId,
            cont->SyncPointShadow );

        /* set-class back to display */
        DcPushSetcl( cont );
    }
    else
    {
        NV_ASSERT( !"no controller pushbuffer" );
    }
}

static void DcPushDelay( DcController *cont, NvU32 usec )
{
    NvU32 val;

    if( cont->pb )
    {
        NVRM_STREAM_PUSH_U( cont->pb,
            NVRM_CH_OPCODE_SET_CLASS( NV_HOST1X_CLASS_ID, 0, 0 ) );

        val = NV_DRF_NUM( NV_CLASS_HOST, DELAY_USEC, NUSEC, usec );

        NVRM_STREAM_PUSH_U( cont->pb,
            NVRM_CH_OPCODE_NONINCR( NV_CLASS_HOST_DELAY_USEC_0, 1 ) );
        NVRM_STREAM_PUSH_U( cont->pb, val );

        DcPushSetcl( cont );
    }
    else
    {
        NV_ASSERT( !"no controller pushbuffer" );
    }
}

/* using just an immediate sync point is actually pretty much always a bug.
 * the display hardware really needs to have an h-flip sync point, but it
 * doesn't, so this will push in a host wait for an arbitrary amount after
 * the immediate so the hardware will have actually started reading from
 * the new surface.
 */
void
DcPushImmediate( DcController *cont )
{
    NvU32 val;

#define HOST_WAIT_HACK  (40)

    if( cont->pb && NVDDK_DISP_CAP_IS_SET( cont->caps,
            NvDdkDispCapBit_Bug_Double_Syncpt_Incr ) )
    {
        // WARNING: bug 580685 -- use 2 immediate commands
        cont->SyncPointShadow += 2;

        DcLatch( cont, NV_FALSE );

        val = NV_DRF_DEF( DC_CMD, GENERAL_INCR_SYNCPT, GENERAL_COND,
                IMMEDIATE )
            | NV_DRF_NUM( DC_CMD, GENERAL_INCR_SYNCPT, GENERAL_INDX,
                cont->SyncPointId );
        DC_WRITE_STREAM( cont, DC_CMD_GENERAL_INCR_SYNCPT_0, val,
            cont->pb );
        DC_WRITE_STREAM( cont, DC_CMD_GENERAL_INCR_SYNCPT_0, val,
            cont->pb );

        DcPushDelay( cont, HOST_WAIT_HACK );
    }
    else if( cont->pb )
    {
        cont->SyncPointShadow++;
        val = NV_DRF_DEF( DC_CMD, GENERAL_INCR_SYNCPT, GENERAL_COND,
                IMMEDIATE )
            | NV_DRF_NUM( DC_CMD, GENERAL_INCR_SYNCPT, GENERAL_INDX,
                cont->SyncPointId );
        DC_WRITE_STREAM( cont, DC_CMD_GENERAL_INCR_SYNCPT_0, val,
            cont->pb );

        DcPushDelay( cont, HOST_WAIT_HACK );
    }
    else
    {
        NV_ASSERT( !"no controller pushbuffer" );
    }

#undef HOST_WAIT_HACK
}

void
DcHal_HwClose( NvU32 controller )
{
    DcController *cont;
    NvU32 i;

    DC_CONTROLLER( controller, cont, return );

    cont->bInitialized = NV_FALSE;

    if( cont->registers )
    {
        /* shut off power, etc. */
        DcSuspend( cont );
    }

    /* unpin/free surfaces */
    for( i = 0; i < DC_HAL_WINDOWS; i++ )
    {
        DcWindow *win = &cont->windows[i];
        DcSurfaceCacheClear( win );
    }

    NvOsIntrMutexDestroy( cont->IntrMutex );
    NvRmStreamFree( &cont->stream );
    NvRmChannelClose( cont->ch );
    NvRmPowerUnRegister( cont->hRm, cont->PowerClientId );
    NvOsSemaphoreDestroy( cont->sem );
    NvOsMutexDestroy( cont->frame_mutex );
    NvOsMutexDestroy( cont->sdMutex );
    NvOsPhysicalMemUnmap( cont->registers, cont->aperture_size );
    NvOsMemset( cont, 0, sizeof(DcController) );
}

void DcHal_HwSuspend( NvU32 controller )
{
    DcController *cont;
    DC_CONTROLLER( controller, cont, return );

    /* clear reopen flag */
    cont->bReopen = NV_FALSE;

    DcSuspend( cont );
}

NvBool DcHal_HwResume( NvU32 controller )
{
    DcController *cont;
    DC_CONTROLLER( controller, cont, return NV_FALSE );

    DcResume( cont );
    return NV_TRUE;
}

/* enable/disable clock and power to the display controller hardware.
 * sets a small initial frequency.
 *
 * don't use the controller structure for param since this is needed before
 * the controller is initialized, in some cases.
 */
void
DcSetClockPower( NvRmDeviceHandle hRm, NvU32 power_id, NvU32 controller,
    NvBool enable, NvBool bReopen )
{
    NvRmFreqKHz freq;

    // get the osc
    freq = NvRmPowerGetPrimaryFrequency( hRm );
    if (!freq)
    {
        NV_DEBUG_PRINTF(("stub RM detected\n"));
        return;
    }

    if( enable )
    {
#if NVRM_AUTO_HOST_ENABLE
        NV_ASSERT_SUCCESS(
            NvRmPowerVoltageControl( hRm,
                NvRmModuleID_GraphicsHost,
                power_id,
                NvRmVoltsUnspecified, NvRmVoltsUnspecified,
                NULL, 0, NULL )
        );
#endif

        // BUG 518736 -- clocks must be enabled before configuring them.
        NV_ASSERT_SUCCESS(
            NvRmPowerModuleClockControl( hRm,
                NVRM_MODULE_ID( NvRmModuleID_Display, controller ),
                power_id,
                NV_TRUE )
        );

        if( !bReopen )
        {
            NV_ASSERT_SUCCESS(
                NvRmPowerModuleClockConfig( hRm,
                    NVRM_MODULE_ID( NvRmModuleID_Display, controller ),
                    power_id,
                    (freq * 95) / 100,
                    (freq * 105) / 100 ,
                    &freq, 1, &freq, (NvRmClockConfigFlags)0 )
            );
        }

        NV_ASSERT_SUCCESS(
            NvRmPowerVoltageControl( hRm,
                NVRM_MODULE_ID( NvRmModuleID_Display, controller ),
                power_id,
                NvRmVoltsUnspecified, NvRmVoltsUnspecified,
                NULL, 0, NULL )
        );
    }
    else
    {
        /* put the display back on a slow clock just to be safe */
        NV_ASSERT_SUCCESS(
            NvRmPowerModuleClockConfig( hRm,
                NVRM_MODULE_ID( NvRmModuleID_Display, controller ),
                power_id,
                (freq * 95) / 100,
                (freq * 105) / 100 ,
                &freq, 1, &freq, (NvRmClockConfigFlags)0 )
        );

        NV_ASSERT_SUCCESS(
            NvRmPowerModuleClockControl( hRm,
                NVRM_MODULE_ID( NvRmModuleID_Display, controller ),
                power_id,
                NV_FALSE )
        );

        NV_ASSERT_SUCCESS(
            NvRmPowerVoltageControl( hRm,
                NVRM_MODULE_ID( NvRmModuleID_Display, controller ),
                power_id,
                NvRmVoltsOff, NvRmVoltsOff,
                NULL, 0, NULL )
        );

#if NVRM_AUTO_HOST_ENABLE
        NV_ASSERT_SUCCESS(
            NvRmPowerVoltageControl( hRm,
                NvRmModuleID_GraphicsHost,
                power_id,
                NvRmVoltsOff, NvRmVoltsOff,
                NULL, 0, NULL )
        );
#endif
    }
}

void
DcSuspend( DcController *cont )
{
    NvU32 val;

    DC_BEGIN( cont );
    DcRegWrSafe( cont );

    /* disable interrupts */
    DC_WRITE( cont, DC_CMD_INT_MASK_0, 0 );
    DC_WRITE( cont, DC_CMD_INT_ENABLE_0, 0 );

    /* disable vsync stuff */
    val = NV_DRF_NUM( DC_CMD, CONT_SYNCPT_VSYNC, VSYNC_EN, 0 );
    DC_WRITE( cont, DC_CMD_CONT_SYNCPT_VSYNC_0, val );

    /* stop display hardware: this may look like a race condition -- the stop
     * vs the wait-for-frame. if the controller stops before the frame
     * begin/end, then the sync point will return immediately, so this should
     * be ok.
     */
    val = NV_DRF_DEF( DC_CMD, DISPLAY_COMMAND, DISPLAY_CTRL_MODE, STOP );
    DC_WRITE( cont, DC_CMD_DISPLAY_COMMAND_0, val );

    DcLatch( cont, NV_FALSE );
    DC_END( cont );
    DC_FLUSH( cont );

    /* this does it's own BEGIN/END */
    DcWaitFrame( cont );

    DC_BEGIN( cont );

    /* turn off power to panel */
    DcSetPanelPower( cont, NV_FALSE );

    DcLatch( cont, NV_FALSE );
    DC_END( cont );
    DC_FLUSH( cont );

    NvRmInterruptUnregister(cont->hRm, cont->InterruptHandle);
    cont->InterruptHandle = NULL;

    NV_DEBUG_CODE(
        val = NvRmChannelSyncPointRead( cont->hRm, cont->SyncPointId );
        NV_ASSERT( val == cont->SyncPointShadow );
    );

    DcSetClockPower( cont->hRm, cont->PowerClientId, cont->index, NV_FALSE,
        NV_FALSE );
    cont->bClocked = NV_FALSE;
}

void
DcResume( DcController *cont )
{
    NvU32 i;
    NvU32 val;

    DcSetClockPower( cont->hRm, cont->PowerClientId, cont->index, NV_TRUE,
        NV_FALSE );
    cont->bClocked = NV_TRUE;

    NV_DEBUG_CODE(
        val = NvRmChannelSyncPointRead( cont->hRm, cont->SyncPointId );
        NV_ASSERT( val == cont->SyncPointShadow );
    );

    /* this must be done before any display register access */
    DcSetLatchMux( cont );

    DC_BEGIN( cont );

    DcRegWrSafe( cont );

    /* set some default filters */
    for( i = 0; i < 3; i++ )
    {
        DcSetWindow( cont, i );
        DC_DIRTY( cont, i );

        /* make sure window header is set before writing filter registers */
        if( !NVDDK_DISP_CAP_IS_SET(
                cont->caps, NvDdkDispCapBit_IndirectRegs ) )
        {
            DC_FLUSH_DIRECT( cont );
        }

        DcExecTable( cont, NV_ARRAY_SIZE(DcLinearFilterTable_Regs),
            DcLinearFilterTable_Regs, DcLinearFilterTable_Vals );
        DcExecTable( cont, NV_ARRAY_SIZE(DcVFilterTable_Regs),
            DcVFilterTable_Regs, DcVFilterTable_Vals );
    }

    /* setup other random stuff */
    DcSetMcConfig( cont );

    /* turn on the panel power */
    DcSetPanelPower( cont, NV_TRUE );

    /* no-stall must be enabled. see bug 580685 */
    if( NVDDK_DISP_CAP_IS_SET( cont->caps,
            NvDdkDispCapBit_Bug_Double_Syncpt_Incr ) )
    {
        val = NV_DRF_NUM( DC_CMD, GENERAL_INCR_SYNCPT_CNTRL,
            GENERAL_INCR_SYNCPT_NO_STALL, 1 );
        DC_WRITE( cont, DC_CMD_GENERAL_INCR_SYNCPT_CNTRL_0, val );
    }

    /* disable underflow line flush.  see bug 588475 */
    if( NVDDK_DISP_CAP_IS_SET( cont->caps,
            NvDdkDispCapBit_Bug_Disable_Underflow_Line_Flush ) )
    {
        val = NV_DRF_DEF( DC_DISP, DISP_MISC_CONTROL, UF_LINE_FLUSH, DISABLE );
        DC_WRITE( cont, DC_DISP_DISP_MISC_CONTROL_0, val );
    }

    /* enable auto-incrementing vsync syncpoint */
    val = NV_DRF_NUM( DC_CMD, CONT_SYNCPT_VSYNC, VSYNC_EN, 1 )
        | NV_DRF_NUM( DC_CMD, CONT_SYNCPT_VSYNC, VSYNC_INDX,
            cont->VsyncSyncPointId );
    DC_WRITE( cont, DC_CMD_CONT_SYNCPT_VSYNC_0, val );

    /* restore interrupt state */
    DcSetInterrupts( cont );

    DcLatch( cont, NV_FALSE );
    DC_END( cont );
    DC_FLUSH( cont );
}

void
DcHal_HwBegin( NvU32 controller )
{
    DcController *cont;
    DC_CONTROLLER( controller, cont, return );

    if( !cont->bClocked )
    {
        NvOsDebugPrintf( "display %d isn't clocked\n", cont->index );
        cont->bFailed = NV_TRUE;
        return;
    }

    NvOsMutexLock( cont->frame_mutex );
    DC_BEGIN( cont );

    DcRegWrSafe( cont );
    cont->bSafe = NV_TRUE;
    cont->bFailed = NV_FALSE;
}

NvBool
DcHal_HwEnd( NvU32 controller, NvU32 flags )
{
    DcController *cont;
    DcWindow *win;
    NvU32 val;
    NvBool nc_display = NV_FALSE;
    NvBool use_te = NV_FALSE;

    DC_CONTROLLER( controller, cont, return NV_FALSE );

    if( cont->bFailed )
    {
        /* caches/stream command buffer should be thrown away and
         * release the hw lock.
         */
        DC_END( cont );
        NvOsMutexUnlock( cont->frame_mutex );
        cont->reg = &cont->regs[0];
        cont->val = &cont->vals[0];
        return NV_FALSE;
    }

    /* handle the window blending */
    if( cont->update_blend )
    {
        DcBlendWindows( cont, NV_FALSE );
        cont->update_blend = NV_FALSE;
    }

    /* check for TE signal */
    if( cont->mode.flags & NVDDK_DISP_MODE_FLAG_USE_TEARING_EFFECT )
    {
        use_te = NV_TRUE;
    }

    val = 0;
    if( use_te )
    {
        val = NV_DRF_DEF( DC_CMD, DISPLAY_COMMAND_OPTION0, MSF_ENABLE,
            ENABLE );
    }
    DC_WRITE( cont, DC_CMD_DISPLAY_COMMAND_OPTION0_0, val );

    /* window enables */
    win = &cont->windows[DC_HAL_WIN_A];
    val = win->win_options;
    DcSetWindow( cont, DC_HAL_WIN_A );
    if( cont->win_enable & DC_WIN_MASK(DC_HAL_WIN_A) )
    {
        val = NV_FLD_SET_DRF_DEF( DC_WIN, B_WIN_OPTIONS, B_WIN_ENABLE,
            ENABLE, val );
    }
    else
    {
        val = NV_FLD_SET_DRF_DEF( DC_WIN, B_WIN_OPTIONS, B_WIN_ENABLE,
            DISABLE, val );
    }
    DC_WRITE( cont, DC_WIN_B_WIN_OPTIONS_0, val );

    win = &cont->windows[DC_HAL_WIN_B];
    val = win->win_options;
    DcSetWindow( cont, DC_HAL_WIN_B );
    if( cont->win_enable & DC_WIN_MASK(DC_HAL_WIN_B) )
    {
        val = NV_FLD_SET_DRF_DEF( DC_WIN, B_WIN_OPTIONS, B_WIN_ENABLE,
            ENABLE, val );
    }
    else
    {
         val = NV_FLD_SET_DRF_DEF( DC_WIN, B_WIN_OPTIONS, B_WIN_ENABLE,
            DISABLE, val );
    }
    DC_WRITE( cont, DC_WIN_B_WIN_OPTIONS_0, val );

    win = &cont->windows[DC_HAL_WIN_C];
    val = win->win_options;
    DcSetWindow( cont, DC_HAL_WIN_C );
    if( cont->win_enable & DC_WIN_MASK(DC_HAL_WIN_C) )
    {
        val = NV_FLD_SET_DRF_DEF( DC_WIN, B_WIN_OPTIONS, B_WIN_ENABLE,
            ENABLE, val );
    }
    else
    {
        val = NV_FLD_SET_DRF_DEF( DC_WIN, B_WIN_OPTIONS, B_WIN_ENABLE,
            DISABLE, val );
    }
    DC_WRITE( cont, DC_WIN_B_WIN_OPTIONS_0, val );

    /* window options */
    val = 0;
    if( cont->win_enable & DC_WIN_MASK(DC_HAL_CURSOR) )
    {
        val = NV_FLD_SET_DRF_DEF( DC_DISP, DISP_WIN_OPTIONS, CURSOR_ENABLE,
            ENABLE, val );
    }
    if( cont->win_enable & DC_WIN_MASK(DC_HAL_HDMI) )
    {
        val = NV_FLD_SET_DRF_DEF( DC_DISP, DISP_WIN_OPTIONS, HDMI_ENABLE,
            ENABLE, val );
    }
    if( cont->win_enable & DC_WIN_MASK(DC_HAL_DSI) )
    {
        val = NV_FLD_SET_DRF_DEF( DC_DISP, DISP_WIN_OPTIONS, DSI_ENABLE,
            ENABLE, val );
    }
    if( cont->win_enable & DC_WIN_MASK(DC_HAL_TVO) )
    {
        val = NV_FLD_SET_DRF_DEF( DC_DISP, DISP_WIN_OPTIONS, TVO_ENABLE,
            ENABLE, val );
        nc_display = NV_TRUE;
    }

    if( use_te )
    {
        nc_display = NV_TRUE;
    }

    DC_WRITE( cont, DC_DISP_DISP_WIN_OPTIONS_0, val );

    /* window command */
    if( cont->win_enable )
    {
        if( !nc_display )
        {
            val = NV_DRF_DEF( DC_CMD, DISPLAY_COMMAND, DISPLAY_CTRL_MODE,
                C_DISPLAY );
        }
        else
        {
            val = NV_DRF_DEF( DC_CMD, DISPLAY_COMMAND, DISPLAY_CTRL_MODE,
                NC_DISPLAY );
        }
    }
    else
    {
        val = NV_DRF_DEF( DC_CMD, DISPLAY_COMMAND, DISPLAY_CTRL_MODE, STOP );
    }

    DC_WRITE( cont, DC_CMD_DISPLAY_COMMAND_0, val );

    DcLatch( cont, NV_FALSE );
    DC_END( cont );
    DC_FLUSH( cont );

    if( flags == NVDDK_DISP_WAIT )
    {
        /* this does its own BEGIN/END */
        DcWaitFrame( cont );
    }

    NvOsMutexUnlock( cont->frame_mutex );
    cont->bSafe = NV_FALSE;
    return NV_TRUE;
}

void
DcHal_HwTriggerFrame( NvU32 controller )
{
    DcController *cont;
    NvU32 val, reg;

    DC_CONTROLLER( controller, cont, return );

    if( (cont->mode.flags & NVDDK_DISP_MODE_FLAG_USE_TEARING_EFFECT) == 0 )
    {
        // FIXME: since there's no begin/end, this may not get reported
        cont->bFailed = NV_TRUE;
        return;
    }

    /* this doesn't need begin/end/latch, etc., but it does need coordination
     * with the dsi ping mechanism, which is what the mutex below is for.
     */
    NvOsMutexLock( cont->frame_mutex );

    HOST_CLOCK_ENABLE( cont );

    reg = DC_CMD_STATE_CONTROL_0 * 4;
    val = NV_DRF_DEF( DC_CMD, STATE_CONTROL, NC_HOST_TRIG_ENABLE, ENABLE )
        | NV_DRF_DEF( DC_CMD, STATE_CONTROL, GENERAL_ACT_REQ, ENABLE );
    DcRegWr( cont, 1, &reg, &val );

    DcWaitFrame( cont );

    HOST_CLOCK_DISABLE( cont );
    NvOsMutexUnlock( cont->frame_mutex );
}

void
DcHal_HwCacheClear( NvU32 controller, NvU32 flags )
{
    DcController *cont;
    NvU32 i, j;

    DC_CONTROLLER( controller, cont, return );

    for( i = 0; i < DC_HAL_WINDOWS; i++ )
    {
        DcWindow *win = &cont->windows[i];
        /* clear unused cached surfaces */
        NvU32 idx = (win->surf_cache_idx - 1) & (DC_HAL_SURF_CACHE_SIZE - 1);

        for( j = 0; j < DC_HAL_SURF_CACHE_SIZE; j++ )
        {
            DcSurfaceCache *c;
            c = &win->surf_cache[j];
            if( j != idx && c->nSurfs )
            {
                DcSurfaceDelete( win, j );
            }
        }
    }
}

void
DcHal_HwSetBaseColorSize( NvU32 controller, NvU32 base )
{
    DcController *cont;

    DC_CONTROLLER( controller, cont, return );
    DC_SAFE( cont, return );

    cont->base = base;
    DcSetColorBase( cont, cont->base );
}

void
DcHal_HwSetDataAlignment( NvU32 controller, NvU32 align )
{
    DcController *cont;
    NvU32 val;

    DC_CONTROLLER( controller, cont, return );
    DC_SAFE( cont, return );

    cont->align = align;
    val = cont->interface_control;
    if( cont->align == NvOdmDispDataAlignment_MSB )
    {
        val = NV_FLD_SET_DRF_DEF( DC_DISP, DISP_INTERFACE_CONTROL,
            DISP_DATA_ALIGNMENT, MSB, val );
    }
    else
    {
        val = NV_FLD_SET_DRF_DEF( DC_DISP, DISP_INTERFACE_CONTROL,
            DISP_DATA_ALIGNMENT, LSB, val );
    }

    cont->interface_control = val;
    DC_WRITE( cont, DC_DISP_DISP_INTERFACE_CONTROL_0, val );
}

void
DcHal_HwSetPwmOutputPin( NvU32 controller, NvU32 PwmOutputPin)
{
    DcController *cont;
    NvU32 port, pin, i, size;
    DcPwmOutputPin OutputPin = DcPwmOutputPin_Unspecified;

    DC_CONTROLLER( controller, cont, return );
    DC_SAFE( cont, return );

    port = NVODM_DISP_GPIO_GET_PORT(PwmOutputPin);
    pin = NVODM_DISP_GPIO_GET_PIN(PwmOutputPin);

    size = NV_ARRAY_SIZE(PwmOutputPinTable);
    for (i = 0; i < size; i++)
    {
        if ((PwmOutputPinTable[i].port == port) &&
            (PwmOutputPinTable[i].pin == pin))
        {
            OutputPin = PwmOutputPinTable[i].InternalOutputPin;
            break;
        }
    }

    cont->PwmOutputPin = OutputPin;
}

void
DcHal_HwSetPinMap( NvU32 controller, NvU32 pinmap )
{
    DcController *cont;

    DC_CONTROLLER( controller, cont, return );
    DC_SAFE( cont, return );

    cont->pinmap = pinmap;

    if( cont->bReopen )
    {
        /* don't reset the pins after they've been initialized */
        return;
    }

    /* reset pins */
    DcExecTable( cont, NV_ARRAY_SIZE(DcDefaultPinTable_Regs),
        DcDefaultPinTable_Regs, DcDefaultPinTable_Vals );

    /* set the pin map */
    switch( pinmap ) {
    case NvOdmDispPinMap_Dual_Rgb18_x_Cpu8:
        break;
    case NvOdmDispPinMap_Dual_Rgb18_Spi4_x_Cpu8:
        break;
    case NvOdmDispPinMap_Dual_Rgb18_x_Cpu9:
        break;
    case NvOdmDispPinMap_Dual_Rgb18_Spi4_x_Cpu9_Shared:
        break;
    case NvOdmDispPinMap_Dual_Rgb18_Spi4_x_Cpu9:
        break;
    case NvOdmDispPinMap_Dual_Rgb24_x_Spi5:
        break;
    case NvOdmDispPinMap_Dual_Rgb24_Spi5_x_Spi5_Shared:
        break;
    case NvOdmDispPinMap_Dual_Rgb24_Spi5_x_Spi5:
        break;
    case NvOdmDispPinMap_Dual_Cpu9_x_Cpu9:
        break;
    case NvOdmDispPinMap_Dual_Cpu18_x_Cpu9:
        break;
    case NvOdmDispPinMap_Single_Cpu9:
        break;
    case NvOdmDispPinMap_Single_Rgb6_Spi4:
        break;
    case NvOdmDispPinMap_Single_Rgb18_Spi4:
        break;
    case NvOdmDispPinMap_Single_Rgb18:
        DcExecTable( cont, NV_ARRAY_SIZE(DcPinMap_Single_Rgb18_Regs),
            DcPinMap_Single_Rgb18_Regs, DcPinMap_Single_Rgb18_Vals );
        break;
    case NvOdmDispPinMap_Single_Cpu18:
        DcExecTable( cont, NV_ARRAY_SIZE(DcPinMap_Single_Cpu18_Regs),
            DcPinMap_Single_Cpu18_Regs, DcPinMap_Single_Cpu18_Vals );
        break;
    case NvOdmDispPinMap_Single_Cpu24:
        DcExecTable( cont, NV_ARRAY_SIZE(DcPinMap_Single_Cpu24_Regs),
            DcPinMap_Single_Cpu24_Regs, DcPinMap_Single_Cpu24_Vals );
        break;
    case NvOdmDispPinMap_Single_Rgb24_Spi5:
        DcExecTable( cont, NV_ARRAY_SIZE(DcPinMap_Single_Rgb24_Spi5_Regs),
            DcPinMap_Single_Rgb24_Spi5_Regs, DcPinMap_Single_Rgb24_Spi5_Vals );
        break;
    case NvOdmDispPinMap_CRT:
        break;
    case NvOdmDispPinMap_Reserved0:
        DcExecTable( cont, NV_ARRAY_SIZE(DcPinMapFramegrabber_Regs),
            DcPinMapFramegrabber_Regs, DcPinMapFramegrabber_Vals );
        break;
    case NvOdmDispPinMap_Reserved1:
        DcExecTable( cont, NV_ARRAY_SIZE(DcPinMapDualFramegrabber_Regs),
            DcPinMapDualFramegrabber_Regs, DcPinMapDualFramegrabber_Vals );
        break;
    case NvOdmDispPinMap_Unspecified:
        break;
    default:
        NV_ASSERT( !"bad pinmap value" );
        cont->bFailed = NV_TRUE;
        return;
    }
}

void DcHal_HwSetPinPolarities( NvU32 controller, NvU32 nPins,
    NvOdmDispPin *pins, NvOdmDispPinPolarity *vals )
{
    DcController *cont;
    NvU32 i;
    NvU32 pol1;
    NvU32 pol3;

    DC_CONTROLLER( controller, cont, return );
    DC_SAFE( cont, return );

    pol1 = NV_RESETVAL( DC_COM, PIN_OUTPUT_POLARITY1 );
    pol1 = NV_DRF_DEF( DC_COM, PIN_OUTPUT_POLARITY1, LSC0_OUTPUT_POLARITY,
            LOW );
    pol3 = NV_RESETVAL( DC_COM, PIN_OUTPUT_POLARITY3 );

    /* note: this won't work for the sonyvga panel's data enable, but that
     * doesn't matter since the default polarities are fine.
     */

    for( i = 0; i < nPins; i++ )
    {
        switch( pins[i] ) {
        case NvOdmDispPin_DataEnable:
            if( vals[i] == NvOdmDispPinPolarity_Low )
            {
                pol3 = NV_FLD_SET_DRF_DEF( DC_COM, PIN_OUTPUT_POLARITY3,
                    LSPI_OUTPUT_POLARITY, LOW, pol3 );
            }
            break;
        case NvOdmDispPin_HorizontalSync:
            if( vals[i] == NvOdmDispPinPolarity_Low )
            {
                pol1 = NV_FLD_SET_DRF_DEF( DC_COM, PIN_OUTPUT_POLARITY1,
                    LHS_OUTPUT_POLARITY, LOW, pol1 );
            }
            break;
        case NvOdmDispPin_VerticalSync:
            if( vals[i] == NvOdmDispPinPolarity_Low )
            {
                pol1 = NV_FLD_SET_DRF_DEF( DC_COM, PIN_OUTPUT_POLARITY1,
                    LVS_OUTPUT_POLARITY, LOW, pol1 );
            }
            break;
        case NvOdmDispPin_PixelClock:
            if( vals[i] == NvOdmDispPinPolarity_High )
            {
                pol1 = NV_FLD_SET_DRF_DEF( DC_COM, PIN_OUTPUT_POLARITY1,
                    LSC0_OUTPUT_POLARITY, HIGH, pol1 );
            }
            break;
        default:
            NV_ASSERT( !"bad pin" );
            break;
        }
    }

    DC_WRITE( cont, DC_COM_PIN_OUTPUT_POLARITY1_0, pol1 );
    DC_WRITE( cont, DC_COM_PIN_OUTPUT_POLARITY3_0, pol3 );
}

void DcHal_HwSetPeriodicInstructions( NvU32 controller, NvU32 nInstructions,
    NvOdmDispCliInstruction *instructions )
{
    DcController *cont;
    NvU32 val;
    NvU32 data[3];
    NvU32 i;
    NvU32 head;
    NvU32 remaining;

    DC_CONTROLLER( controller, cont, return );
    DC_SAFE( cont, return );

    NvOsMemset( data, 0, sizeof(data) );

    /* setup vpluse1 */
    val = cont->signal_options0;
    val = NV_FLD_SET_DRF_DEF( DC_DISP, DISP_SIGNAL_OPTIONS0,
        V_PULSE1_ENABLE, ENABLE, val );
    cont->signal_options0 = val;
    DC_WRITE( cont, DC_DISP_DISP_SIGNAL_OPTIONS0_0, val );

    val = NV_DRF_DEF( DC_DISP, V_PULSE1_CONTROL, V_PULSE1_POLARITY, LOW )
        | NV_DRF_DEF( DC_DISP, V_PULSE1_CONTROL, V_PULSE1_DELAY, NODELAY )
        | NV_DRF_DEF( DC_DISP, V_PULSE1_CONTROL, V_PULSE1_LAST, START_A )
        | NV_DRF_NUM( DC_DISP, V_PULSE1_CONTROL, V_PULSE1_H_POSITION, 0 );
    DC_WRITE( cont, DC_DISP_V_PULSE1_CONTROL_0, val );

    val = NV_DRF_NUM( DC_DISP, V_PULSE1_POSITION_A, V_PULSE1_START_A, 0 )
        | NV_DRF_NUM( DC_DISP, V_PULSE1_POSITION_A, V_PULSE1_END_A, 1 );
    DC_WRITE( cont, DC_DISP_V_PULSE1_POSITION_A_0, val );
    DC_WRITE( cont, DC_DISP_V_PULSE1_POSITION_B_0, 0 );
    DC_WRITE( cont, DC_DISP_V_PULSE1_POSITION_C_0, 0 );

    /* setup initialization sequence:
     * use vpluse1
     * DC is inverted and then AND-ed to vertical signal.
     */
    val = NV_DRF_DEF( DC_DISP, INIT_SEQ_CONTROL, SEND_INIT_SEQUENCE, ENABLE )
        | NV_DRF_DEF( DC_DISP, INIT_SEQ_CONTROL, INIT_SEQUENCE_MODE,
            PLCD_INIT )
        | NV_DRF_DEF( DC_DISP, INIT_SEQ_CONTROL, INIT_SEQ_DC_SIGNAL, VPULSE1 )
        | NV_DRF_NUM( DC_DISP, INIT_SEQ_CONTROL, INIT_SEQ_DC_CONTROL, 0 )
        | NV_DRF_NUM( DC_DISP, INIT_SEQ_CONTROL, FRAME_INIT_SEQ_CYCLES,
            nInstructions );
    DC_WRITE( cont, DC_DISP_INIT_SEQ_CONTROL_0, val );

    /* instructions 1-6 go into SPI_INIT_SEQ_DATA_A through C (assuming
     * that this always uses the parallel 18 bit interface). each instruction
     * is 18 bits -- need to pad the top 2 bits since the api only exposes
     * 16 bit instructions.
     *
     * control for each instuction goes into D starting at bit 12.
     *
     * control format - 3 bit tuple:
     * bit 0: lsc0 enable (always set)
     * bit 1: lsc1 enable (ignore, for sub panel)
     * bit 2: data/command - 1 for command.
     */
    val = 0;
    for( i = 0; i < nInstructions; i++ )
    {
        NvU32 tmp;
        tmp = ((instructions[i].Command ? 1 : 0) << 2 ) | 1;
        val |= (tmp << (3*i));
    }

    val <<= 12;
    DC_WRITE( cont, DC_DISP_SPI_INIT_SEQ_DATA_D_0, val );

    head = 0;
    remaining = 32;
    for( i = 0; i < nInstructions; i++ )
    {
        NvU32 tmp;
        NvU32 width;
        NvU32 mask = 0xFFFF;

        width = NV_MIN( remaining, 16 );

        mask >>= (16 - width);
        tmp = (instructions[i].Data & mask) << (32 - remaining);
        data[head/32] |= tmp;

        if( remaining < 18 )
        {
            /* overlow to the next word */
            mask = 0xFFFF;
            width = (16 - remaining);

            mask >>= (16 - width);
            mask <<= remaining;
            tmp = (instructions[i].Data & mask) >> remaining;
            data[(head/32)+1] |= tmp;

            /* make sure to pad out to 18 bits */
            remaining = 32 - (18 - remaining);
        }
        else
        {
            remaining -= 18;
        }

        head += 18;
    }

    DC_WRITE( cont, DC_DISP_SPI_INIT_SEQ_DATA_A_0, data[0] );
    DC_WRITE( cont, DC_DISP_SPI_INIT_SEQ_DATA_B_0, data[1] );
    DC_WRITE( cont, DC_DISP_SPI_INIT_SEQ_DATA_C_0, data[2] );
}

/**
 * Generate an h and v sync with pulse1 for the dual framegrabber config --
 * use lhp1 and lvp1 for Display A.
 */
static void
DcFakeSync( DcController *cont )
{
    NvU32 val;

    NV_ASSERT( cont->mode.timing.Horiz_DispActive &&
               cont->mode.timing.Vert_DispActive );

    val = cont->signal_options0;
    val = NV_FLD_SET_DRF_DEF( DC_DISP, DISP_SIGNAL_OPTIONS0,
        H_PULSE1_ENABLE, ENABLE, val );
    val = NV_FLD_SET_DRF_DEF( DC_DISP, DISP_SIGNAL_OPTIONS0,
        V_PULSE1_ENABLE, ENABLE, val );
    cont->signal_options0 = val;
    DC_WRITE( cont, DC_DISP_DISP_SIGNAL_OPTIONS0_0, val );

    val = NV_DRF_DEF( DC_DISP, H_PULSE1_CONTROL, H_PULSE1_MODE, NORMAL )
        | NV_DRF_DEF( DC_DISP, H_PULSE1_CONTROL, H_PULSE1_POLARITY, HIGH )
        | NV_DRF_DEF( DC_DISP, H_PULSE1_CONTROL, H_PULSE1_V_QUAL, ALWAYS )
        | NV_DRF_DEF( DC_DISP, H_PULSE1_CONTROL, H_PULSE1_LAST, END_A );
    DC_WRITE( cont, DC_DISP_H_PULSE1_CONTROL_0, val );

    val = NV_DRF_NUM( DC_DISP, H_PULSE1_POSITION_A,
            H_PULSE1_START_A, cont->mode.timing.Horiz_RefToSync )
        | NV_DRF_NUM( DC_DISP, H_PULSE1_POSITION_A,
            H_PULSE1_END_A, cont->mode.timing.Horiz_RefToSync +
                cont->mode.timing.Horiz_DispActive );
    DC_WRITE( cont, DC_DISP_H_PULSE1_POSITION_A_0, val );

    val = NV_DRF_DEF( DC_DISP, V_PULSE1_CONTROL, V_PULSE1_POLARITY, HIGH )
        | NV_DRF_DEF( DC_DISP, V_PULSE1_CONTROL, V_PULSE1_DELAY, NODELAY )
        | NV_DRF_DEF( DC_DISP, V_PULSE1_CONTROL, V_PULSE1_LAST, END_A );
    DC_WRITE( cont, DC_DISP_V_PULSE1_CONTROL_0, val );

    val = NV_DRF_NUM( DC_DISP, V_PULSE1_POSITION_A,
            V_PULSE1_START_A, cont->mode.timing.Vert_RefToSync )
        | NV_DRF_NUM( DC_DISP, V_PULSE1_POSITION_A,
            V_PULSE1_END_A, cont->mode.timing.Vert_RefToSync +
                cont->mode.timing.Vert_DispActive );
    DC_WRITE( cont, DC_DISP_V_PULSE1_POSITION_A_0, val );
}

void
DcModeFlags( DcController *cont, NvDdkDispModeFlags flags )
{
    NvU32 val;

    /* shift clock setup */
    if( flags & NvDdkDispModeFlag_DisableClockInBlank )
    {
        val = NV_DRF_DEF(DC_DISP, SHIFT_CLOCK_OPTIONS, SC0_H_QUALIFIER,
                HACTIVE)
            | NV_DRF_DEF(DC_DISP, SHIFT_CLOCK_OPTIONS, SC0_V_QUALIFIER,
                VACTIVE)
            | NV_DRF_DEF(DC_DISP, SHIFT_CLOCK_OPTIONS, SC1_H_QUALIFIER,
                NO_HQUAL)
            | NV_DRF_DEF(DC_DISP, SHIFT_CLOCK_OPTIONS, SC1_V_QUALIFIER,
                NO_VQUAL);
    }
    else
    {
        /* continuous mode */
        val = NV_DRF_DEF(DC_DISP, SHIFT_CLOCK_OPTIONS, SC0_H_QUALIFIER,
                NO_HQUAL)
            | NV_DRF_DEF(DC_DISP, SHIFT_CLOCK_OPTIONS, SC0_V_QUALIFIER,
                NO_VQUAL)
            | NV_DRF_DEF(DC_DISP, SHIFT_CLOCK_OPTIONS, SC1_H_QUALIFIER,
                NO_HQUAL)
            | NV_DRF_DEF(DC_DISP, SHIFT_CLOCK_OPTIONS, SC1_V_QUALIFIER,
                NO_VQUAL);
    }
    DC_WRITE( cont, DC_DISP_SHIFT_CLOCK_OPTIONS_0, val );

    if( flags & NvDdkDispModeFlag_DataEnableMode )
    {
        /* disable lhs and lvs */
        val = NV_DRF_DEF( DC_COM, PIN_OUTPUT_ENABLE1, LD16_OUTPUT_ENABLE,
                ENABLE )
            | NV_DRF_DEF( DC_COM, PIN_OUTPUT_ENABLE1, LD17_OUTPUT_ENABLE,
                ENABLE )
            | NV_DRF_DEF( DC_COM, PIN_OUTPUT_ENABLE1, LPW0_OUTPUT_ENABLE,
                ENABLE )
            | NV_DRF_DEF( DC_COM, PIN_OUTPUT_ENABLE1, LPW1_OUTPUT_ENABLE,
                ENABLE )
            | NV_DRF_DEF( DC_COM, PIN_OUTPUT_ENABLE1, LPW2_OUTPUT_ENABLE,
                ENABLE )
            | NV_DRF_DEF( DC_COM, PIN_OUTPUT_ENABLE1, LSC0_OUTPUT_ENABLE,
                ENABLE )
            | NV_DRF_DEF( DC_COM, PIN_OUTPUT_ENABLE1, LSC1_OUTPUT_ENABLE,
                ENABLE )
            | NV_DRF_DEF( DC_COM, PIN_OUTPUT_ENABLE1, LVS_OUTPUT_ENABLE,
                DISABLE )
            | NV_DRF_DEF( DC_COM, PIN_OUTPUT_ENABLE1, LHS_OUTPUT_ENABLE,
                DISABLE );
    }
    else
    {
        val = NV_DRF_DEF( DC_COM, PIN_OUTPUT_ENABLE1, LD16_OUTPUT_ENABLE,
                ENABLE )
            | NV_DRF_DEF( DC_COM, PIN_OUTPUT_ENABLE1, LD17_OUTPUT_ENABLE,
                ENABLE )
            | NV_DRF_DEF( DC_COM, PIN_OUTPUT_ENABLE1, LPW0_OUTPUT_ENABLE,
                ENABLE )
            | NV_DRF_DEF( DC_COM, PIN_OUTPUT_ENABLE1, LPW1_OUTPUT_ENABLE,
                ENABLE )
            | NV_DRF_DEF( DC_COM, PIN_OUTPUT_ENABLE1, LPW2_OUTPUT_ENABLE,
                ENABLE )
            | NV_DRF_DEF( DC_COM, PIN_OUTPUT_ENABLE1, LSC0_OUTPUT_ENABLE,
                ENABLE )
            | NV_DRF_DEF( DC_COM, PIN_OUTPUT_ENABLE1, LSC1_OUTPUT_ENABLE,
                ENABLE )
            | NV_DRF_DEF( DC_COM, PIN_OUTPUT_ENABLE1, LVS_OUTPUT_ENABLE,
                ENABLE )
            | NV_DRF_DEF( DC_COM, PIN_OUTPUT_ENABLE1, LHS_OUTPUT_ENABLE,
                ENABLE );
    }
    DC_WRITE( cont, DC_COM_PIN_OUTPUT_ENABLE1_0, val );

    /* hack for sony vga panel which has the data enable on a different pin */
    if( flags & NvDdkDispModeFlag_DataEnableMode &&
        cont->pinmap == NvOdmDispPinMap_Single_Rgb18  )
    {
        /* put data enable onto the lsc1 pin */
        val = cont->output_select3;
        val = NV_FLD_SET_DRF_NUM( DC, COM_PIN_OUTPUT_SELECT3,
                LSC1_OUTPUT_SELECT, 2, val );
        DC_WRITE( cont, DC_COM_PIN_OUTPUT_SELECT3_0, val );
        cont->output_select3 = val;

        /* disable default DE pin */
        val = NV_DRF_NUM( DC_COM, PIN_OUTPUT_SELECT6, LSPI_OUTPUT_SELECT, 0 );
        DC_WRITE( cont, DC_COM_PIN_OUTPUT_SELECT6_0, val );
    }
}

void
DcHal_HwSetMode( NvU32 controller, NvOdmDispDeviceMode *mode,
    NvDdkDispModeFlags flags )
{
    DcController *cont;
    NvError err;
    NvU32 val;
    NvOdmDispDeviceTiming *timing;
    NvU32 max_freq = 0;
    NvU32 div = 0;
    NvU32 tolerance;
    NvRmClockConfigFlags clock_flags = (NvRmClockConfigFlags)0;

    DC_CONTROLLER( controller, cont, return );
    DC_SAFE( cont, return );

    timing = &mode->timing;

    /* verify DC's timing constraints */
    NV_ASSERT( (timing->Horiz_RefToSync + timing->Horiz_SyncWidth +
        timing->Horiz_BackPorch) > 11 );
    NV_ASSERT( (timing->Vert_RefToSync + timing->Vert_SyncWidth +
        timing->Vert_BackPorch) > 1 );
    NV_ASSERT( (timing->Vert_FrontPorch + timing->Vert_SyncWidth +
        timing->Vert_BackPorch) > 1 );
    NV_ASSERT( timing->Vert_SyncWidth >= 1 );
    NV_ASSERT( timing->Horiz_SyncWidth >= 1 );
    NV_ASSERT( timing->Vert_RefToSync >= 1 );
    NV_ASSERT( timing->Horiz_DispActive >= 16 );
    NV_ASSERT( timing->Vert_DispActive >= 16 );

    /* save the mode for later operations, if needed */
    cont->mode = *mode;

    val = 0;
    DC_WRITE( cont, DC_DISP_DISP_TIMING_OPTIONS_0, val );

    /* ref to sync */
    val = NV_DRF_NUM( DC_DISP, REF_TO_SYNC, H_REF_TO_SYNC,
            timing->Horiz_RefToSync )
        | NV_DRF_NUM( DC_DISP, REF_TO_SYNC, V_REF_TO_SYNC,
            timing->Vert_RefToSync );
    DC_WRITE( cont, DC_DISP_REF_TO_SYNC_0, val );

    /* sync width */
    val = NV_DRF_NUM( DC_DISP, SYNC_WIDTH, H_SYNC_WIDTH,
            timing->Horiz_SyncWidth )
        | NV_DRF_NUM( DC_DISP, SYNC_WIDTH, V_SYNC_WIDTH,
            timing->Vert_SyncWidth );
    DC_WRITE( cont, DC_DISP_SYNC_WIDTH_0, val );

    /* back porch */
    val = NV_DRF_NUM( DC_DISP, BACK_PORCH, H_BACK_PORCH,
            (NvU32)timing->Horiz_BackPorch )
        | NV_DRF_NUM( DC_DISP, BACK_PORCH, V_BACK_PORCH,
            (NvU32)timing->Vert_BackPorch );
    DC_WRITE( cont, DC_DISP_BACK_PORCH_0, val );

    /* disp active */
    val = NV_DRF_NUM( DC_DISP, DISP_ACTIVE, H_DISP_ACTIVE,
            timing->Horiz_DispActive )
        | NV_DRF_NUM( DC_DISP, DISP_ACTIVE, V_DISP_ACTIVE,
            timing->Vert_DispActive );
    DC_WRITE( cont, DC_DISP_DISP_ACTIVE_0, val );

    /* front porch */
    val = NV_DRF_NUM( DC_DISP, FRONT_PORCH, H_FRONT_PORCH,
            timing->Horiz_FrontPorch )
        | NV_DRF_NUM( DC_DISP, FRONT_PORCH, V_FRONT_PORCH,
            timing->Vert_FrontPorch );
    DC_WRITE( cont,  DC_DISP_FRONT_PORCH_0, val );

    DcModeFlags( cont, flags );

    /* data enable */
    // FIXME: handle dual display (smart panel uses de as chip select)
    val = NV_DRF_DEF( DC_DISP, DATA_ENABLE_OPTIONS, DE_CONTROL, NORMAL )
        | NV_DRF_DEF( DC_DISP, DATA_ENABLE_OPTIONS, DE_SELECT, ACTIVE );
    DC_WRITE( cont, DC_DISP_DATA_ENABLE_OPTIONS_0, val );

    val = cont->interface_control;

    /**
     * reserved1 is a hack, need to handle the data alignment based on the
     * controller number since the pins are shared.
     */
    if( cont->pinmap == NvOdmDispPinMap_Reserved1 )
    {
        /* dual framegrabber, need 3 clocks per pixel */
        val = NV_FLD_SET_DRF_DEF( DC_DISP, DISP_INTERFACE_CONTROL,
            DISP_DATA_FORMAT, DF1P3C24B, val );
        /* displayb is lsb, display is msb */
        if( cont->index == 1 )
        {
            val = NV_FLD_SET_DRF_DEF( DC_DISP, DISP_INTERFACE_CONTROL,
                DISP_DATA_ALIGNMENT, LSB, val );
        }
        else
        {
            /* setup the h and v sync */
            DcFakeSync( cont );

            val = NV_FLD_SET_DRF_DEF( DC_DISP, DISP_INTERFACE_CONTROL,
                DISP_DATA_ALIGNMENT, MSB, val );
        }
    }
    else
    {
        val = NV_FLD_SET_DRF_DEF( DC_DISP, DISP_INTERFACE_CONTROL,
            DISP_DATA_FORMAT, DF1P1C, val );
        if( cont->align == NvOdmDispDataAlignment_MSB )
        {
            val = NV_FLD_SET_DRF_DEF( DC_DISP, DISP_INTERFACE_CONTROL,
                DISP_DATA_ALIGNMENT, MSB, val );
        }
        else
        {
            val = NV_FLD_SET_DRF_DEF( DC_DISP, DISP_INTERFACE_CONTROL,
                DISP_DATA_ALIGNMENT, LSB, val );
        }
    }

    cont->interface_control = val;
    DC_WRITE( cont, DC_DISP_DISP_INTERFACE_CONTROL_0, val );

    /* if tearing effect is used, need to enable lspi input */
    if( mode->flags & NVDDK_DISP_MODE_FLAG_USE_TEARING_EFFECT )
    {
        val = NV_DRF_DEF( DC_COM, PIN_OUTPUT_ENABLE3, LSPI_OUTPUT_ENABLE,
                DISABLE ) ;
        DC_WRITE( cont, DC_COM_PIN_OUTPUT_ENABLE3_0, val );
        // FIXME: input controls are removed after ap20
#ifdef NV_INPUT_CTRL_EXISTS
        val = NV_DRF_DEF( DC_COM, PIN_INPUT_ENABLE3, LSPI_INPUT_ENABLE,
                ENABLE );
        DC_WRITE( cont, DC_COM_PIN_INPUT_ENABLE3_0, val );
#endif
    }

    /* panel frequency should already be calculated */
    NV_ASSERT( mode->frequency );
    cont->panel_freq = mode->frequency;

    cont->bMipi = NV_FALSE;
    if( flags & NvDdkDispModeFlag_CRT )
    {
        clock_flags = NvRmClockConfig_SubConfig;
    }
    else if( flags & NvDdkDispModeFlag_MIPI )
    {
        clock_flags = NvRmClockConfig_MipiSync;
        cont->bMipi = NV_TRUE;
    }

    /* verify that the chip can support the requested frequency */
    max_freq = NvRmPowerModuleGetMaxFrequency( cont->hRm,
        NVRM_MODULE_ID( NvRmModuleID_Display, cont->index ) );

    if ( max_freq == 0 )
        goto enableBackground;

    if( max_freq < cont->panel_freq )
    {
        cont->bFailed = NV_TRUE;
        return;
    }

    /* The PLL frequency will be returned as the set frequency rather than
     * the requested pixel clock since display has its own divider.
     *
     * The usual clock tolerance is 10%. CRTs typically need much more strict
     * tolerance (2% works).
     *
     * HDMI requires even tighter tolorance +- 0.5%
     */

    tolerance = 1095; // -0.5% to +9.5%

    if( (flags & NvDdkDispModeFlag_CRT)                      ||
        (cont->mode.flags & NVDDK_DISP_MODE_FLAG_TYPE_DTD) )
    {
        tolerance = 1015;  // -0.5% to +1.5%
    }

    if ( (cont->mode.flags & NVDDK_DISP_MODE_FLAG_TYPE_HDMI) ||
         (cont->mode.flags & NVDDK_DISP_MODE_FLAG_TYPE_VESA) )
    {
	    tolerance = 1005;    //  -0.5% to +0.5%
    }

    NV_DEBUG_PRINTF(("dc_hal.c freq:%d   tol:%d flags:0x%lx \n",
                        cont->panel_freq, tolerance,cont->mode.flags));

    err = NvRmPowerModuleClockConfig( cont->hRm,
        NVRM_MODULE_ID( NvRmModuleID_Display, cont->index ),
        cont->PowerClientId,
        (cont->panel_freq * 995) / 1000,
        (cont->panel_freq * tolerance) / 1000,
        &cont->panel_freq, 1, &cont->freq, clock_flags );
    if( err != NvSuccess )
    {
        NV_ASSERT( !"DcHal_HwSetMode failed" );
        cont->bFailed = NV_TRUE;
        return;
    }

    /* frequency will be 1khz on quickturn -- don't want to divide clocks on
     * quickturn since it's too slow.
     */
    if( cont->freq > 1 )
    {
        NvU32 panel_freq = cont->panel_freq;

        /* need to put in some margin */
        NV_ASSERT( ((panel_freq * 99) / 100) <= cont->freq );

        /* calculate divider -- hardware is in half steps */
        div = (((cont->freq * 2) + (panel_freq / 2)) / panel_freq) - 2;
    }
    else
    {
        div = 1;
    }

    if( cont->pinmap == NvOdmDispPinMap_Reserved1 )
    {
        val = NV_DRF_DEF( DC_DISP, DISP_CLOCK_CONTROL, PIXEL_CLK_DIVIDER,
            PCD3 );
    }
    else
    {
        val = NV_DRF_DEF( DC_DISP, DISP_CLOCK_CONTROL, PIXEL_CLK_DIVIDER,
            PCD1 );
    }
    val = NV_FLD_SET_DRF_NUM( DC_DISP, DISP_CLOCK_CONTROL, SHIFT_CLK_DIVIDER,
        div, val );
    DC_WRITE( cont, DC_DISP_DISP_CLOCK_CONTROL_0, val );

enableBackground:
    /* enable the background color */
    DcEnable( cont, DC_HAL_BACKGROUND, NV_TRUE );
}

/** assumes the window indirection register is set */
void DcSetSurfaceAddress( DcController *cont, DcWindow *win, NvRect *src,
    NvRmSurface *surf, NvU32 count )
{
    NvU32 h_off;
    NvU32 v_off;
    NvU32 x, y;
    NvU32 offset;
    NvU32 i;

    /* start address, handle planar formats. */
    for( i = 0; i < count; i++ )
    {
        NvU32 val;
        NvRmSurface *s = surf + i;

        val = win->surf_addrs[i];
        val += s->Offset;

        NV_DEBUG_CODE(
            if( s->Layout == NvRmSurfaceLayout_Tiled )
            {
                /* address must be 256 byte aligned */
                NV_ASSERT( (val & 255) == 0 );
            }
        );

        /* need to check for cont->pb since flip() can use a pushbuffer
         * regardless of the cap bit.
         */
        switch( s->ColorFormat ) {
        case NvColorFormat_U8:
            if( cont->pb )
            {
                DC_WRITE_STREAM( cont, DC_WINBUF_B_START_ADDR_U_0, val,
                    cont->pb );
            }
            else
            {
                DC_WRITE_DIRECT( cont, DC_WINBUF_B_START_ADDR_U_0, val );
            }
            break;
        case NvColorFormat_V8:
            if( cont->pb )
            {
                DC_WRITE_STREAM( cont, DC_WINBUF_B_START_ADDR_V_0, val,
                    cont->pb );
            }
            else
            {
                DC_WRITE_DIRECT( cont, DC_WINBUF_B_START_ADDR_V_0, val );
            }
            break;
        default:
            if( cont->pb )
            {
                DC_WRITE_STREAM( cont, DC_WINBUF_B_START_ADDR_0, val,
                    cont->pb );
            }
            else
            {
                DC_WRITE_DIRECT( cont, DC_WINBUF_B_START_ADDR_0, val );
            }
            break;
        }
    }

    /* address offsets -- also handle h/v mirroring */

    // round down
    x = (src->left) & ~1;
    y = (src->top) & ~1;

    offset = NvRmSurfaceComputeOffset( surf, x, y );

    /* v_off is the y coordinate.
     * h_off is a byte offset.
     */

    if( surf->Layout == NvRmSurfaceLayout_Tiled )
    {
        NvU32 row_num, row_rem, rem;
        NvU32 x0, y0;

        #define SUBTILE_SIZE \
            (NVRM_SURFACE_SUB_TILE_WIDTH * NVRM_SURFACE_SUB_TILE_HEIGHT)

        /* remove full lines */
        row_num = offset / surf->Pitch / NVRM_SURFACE_SUB_TILE_HEIGHT;
        row_rem = offset - (row_num * surf->Pitch *
            NVRM_SURFACE_SUB_TILE_HEIGHT);

        /* get base y coord */
        y0 = row_num * NVRM_SURFACE_SUB_TILE_HEIGHT;

        /* number of full tiles. gives the base X coord in bytes */
        x0 = (row_rem / SUBTILE_SIZE) * NVRM_SURFACE_SUB_TILE_WIDTH;

        /* get X offset into tile, add into the base X coord */
        rem = row_rem % SUBTILE_SIZE;
        h_off = x0 + (rem % NVRM_SURFACE_SUB_TILE_WIDTH);

        /* get Y offset */
        v_off = y0 + (rem / NVRM_SURFACE_SUB_TILE_WIDTH);

        #undef SUBTILE_SIZE
    }
    else
    {
        v_off = y;
        h_off = offset - (v_off * surf->Pitch);
    }

    if( win->invert_horizontal )
    {
        NvU32 Bpp;
        Bpp = (NV_COLOR_GET_BPP(surf->ColorFormat) / 8);
        h_off += (src->right - src->left) * Bpp;
        h_off -= Bpp;
    }
    if( win->invert_vertical )
    {
        v_off += (src->bottom - src->top) - 1;
    }

    if( cont->pb )
    {
        DC_WRITE_STREAM( cont, DC_WINBUF_B_ADDR_H_OFFSET_0, h_off, cont->pb );
        DC_WRITE_STREAM( cont, DC_WINBUF_B_ADDR_V_OFFSET_0, v_off, cont->pb );
    }
    else
    {
        DC_WRITE_DIRECT( cont, DC_WINBUF_B_ADDR_H_OFFSET_0, h_off );
        DC_WRITE_DIRECT( cont, DC_WINBUF_B_ADDR_V_OFFSET_0, v_off );
    }
}

void DcSurfaceDelete( DcWindow *win, NvU32 idx )
{
    NvU32 i;
    DcSurfaceCache *c = &win->surf_cache[idx];

    NV_ASSERT( c->nSurfs );

    NvRmMemUnpinMult( c->hSurfs, c->nSurfs );
    for( i = 0; i < c->nSurfs; i++ )
    {
        NvRmMemHandleFree( c->hSurfs[i] );
    }
    NvOsMemset( c, 0, sizeof(DcSurfaceCache) );
}

void DcSurfaceCacheClear( DcWindow *win )
{
    NvU32 i;
    DcSurfaceCache *c;

    for( i = 0; i < DC_HAL_SURF_CACHE_SIZE; i++ )
    {
        c = &win->surf_cache[i];
        if( c->nSurfs )
        {
            DcSurfaceDelete( win, i );
        }
    }

    win->surf_cache_idx = 0;
}

void DcSurfaceCacheAdd( DcWindow *win )
{
    DcSurfaceCache *c;
    NvU32 idx = win->surf_cache_idx;

    c = &win->surf_cache[idx];

    /* check for previously cached surface...kick it out of the cache */
    if( c->nSurfs )
    {
        DcSurfaceDelete( win, idx );
    }

    NvOsMemcpy( c->hSurfs, win->hSurfs, sizeof(win->hSurfs) );
    NvOsMemcpy( c->surf_addrs, win->surf_addrs, sizeof(win->surf_addrs) );
    c->nSurfs = win->nSurfs;
    win->surf_cache_idx = (idx + 1) & (DC_HAL_SURF_CACHE_SIZE - 1);
}

NvBool DcSurfaceCacheFind( DcWindow *win, NvRmSurface *s, NvU32 count )
{
    NvU32 i, j;
    DcSurfaceCache *c;
    NvBool found;

    found = NV_FALSE;
    for( i = 0; i < DC_HAL_SURF_CACHE_SIZE; i++ )
    {
        NvBool skip = NV_FALSE;
        c = &win->surf_cache[i];
        if( c->nSurfs == count )
        {
            for( j = 0; j < count; j++ )
            {
                if( c->hSurfs[j] != (s + j)->hMem )
                {
                    skip = NV_TRUE;
                    break;
                }
            }

            if( skip ) continue;

            found = NV_TRUE;
            break;
        }
    }

    if( !found )
    {
        return NV_FALSE;
    }

    NvOsMemcpy( win->hSurfs, c->hSurfs, sizeof(win->hSurfs) );
    NvOsMemcpy( win->surf_addrs, c->surf_addrs, sizeof(win->surf_addrs) );
    win->nSurfs = c->nSurfs;
    return NV_TRUE;
}

void
DcHal_HwSetWindowSurface( NvU32 controller, NvU32 window,
    NvRmSurface *surf, NvU32 count, NvRect *src, NvRect *dest, NvPoint *loc,
    NvU32 rot, NvDdkDispMirror mirror )
{
    DcController *cont;
    DcWindow *win;
    NvU32 val;
    NvU32 width;
    NvU32 height;
    NvU32 surf_width;
    NvU32 surf_height;
    NvU32 h_dda, v_dda;
    NvU32 i;
    NvBool b;

    NV_ASSERT( surf );
    NV_ASSERT( src );
    NV_ASSERT( dest );
    NV_ASSERT( loc );

    DC_CONTROLLER( controller, cont, return );
    DC_SAFE( cont, return );
    DC_DIRTY( cont, window );

    win = &cont->windows[window];

    /* this will increment the memory handle reference count and keep the
     * surfaces pinned.
     */
    for( i = 0; i < count; i++ )
    {
        NvRmSurface *s = surf + i;
        INCR_MEM_REF( s->hMem );
    }

    /* unpin/free the previous surface(s) */
    DcSurfaceCacheClear( win );

    if( !surf->hMem )
    {
        DcEnable( cont, window, NV_FALSE );

        win->hSurfs[window] = 0;
        win->nSurfs = 0;
        return;
    }

    /* pin/save the new surface -- prevent bad things from happening */
    for( i = 0; i < count; i++ )
    {
        NvRmSurface *s = surf + i;
        win->hSurfs[i] = s->hMem;
    }
    win->nSurfs = count;
    NvRmMemPinMult( win->hSurfs, win->surf_addrs, win->nSurfs );

    /* cache the surface in case it's used for flipping */
    DcSurfaceCacheAdd( win );

    DcEnable( cont, window, NV_TRUE );

    /* set hardware window */
    DcSetWindow( cont, window );

    /* verify/set color depth */
    b = DcSetColorDepth( cont, win, surf, count );
    if( !b )
    {
        NV_ASSERT( !"DcHal_HwSetWindowSurface color depth failed" );
        cont->bFailed = NV_TRUE;
        return;
    }

    /* mirroring. */
    win->invert_vertical = NV_FALSE;
    win->invert_horizontal = NV_FALSE;
    val = win->win_options;
    switch( mirror ) {
    case NvDdkDispMirror_None:
        val = NV_FLD_SET_DRF_DEF( DC_WIN, B_WIN_OPTIONS, B_H_DIRECTION,
            INCREMENT, val );
        val = NV_FLD_SET_DRF_DEF( DC_WIN, B_WIN_OPTIONS, B_V_DIRECTION,
            INCREMENT, val );
        break;
    case NvDdkDispMirror_Horizontal:
        val = NV_FLD_SET_DRF_DEF( DC_WIN, B_WIN_OPTIONS, B_H_DIRECTION,
            DECREMENT, val );
        val = NV_FLD_SET_DRF_DEF( DC_WIN, B_WIN_OPTIONS, B_V_DIRECTION,
            INCREMENT, val );
        win->invert_horizontal = NV_TRUE;
        break;
    case NvDdkDispMirror_Vertical:
        val = NV_FLD_SET_DRF_DEF( DC_WIN, B_WIN_OPTIONS, B_H_DIRECTION,
            INCREMENT, val );
        val = NV_FLD_SET_DRF_DEF( DC_WIN, B_WIN_OPTIONS, B_V_DIRECTION,
            DECREMENT, val );
        win->invert_vertical = NV_TRUE;
        break;
    case NvDdkDispMirror_Both:
        val = NV_FLD_SET_DRF_DEF( DC_WIN, B_WIN_OPTIONS, B_H_DIRECTION,
            DECREMENT, val );
        val = NV_FLD_SET_DRF_DEF( DC_WIN, B_WIN_OPTIONS, B_V_DIRECTION,
            DECREMENT, val );
        win->invert_vertical = NV_TRUE;
        win->invert_horizontal = NV_TRUE;
        break;
    default:
        NV_ASSERT( !"DcHal_HwSetWindowSurface mirror options failed" );
        cont->bFailed = NV_TRUE;
        return;
    }
    win->win_options = val;
    DC_WRITE( cont, DC_WIN_B_WIN_OPTIONS_0, val );

    /* position */
    val = NV_DRF_NUM( DC_WIN, B_POSITION, B_H_POSITION, dest->left + loc->x )
        | NV_DRF_NUM( DC_WIN, B_POSITION, B_V_POSITION, dest->top + loc->y );
    DC_WRITE( cont, DC_WIN_B_POSITION_0, val );

    /* pre-scaled size (in bytes) */
    surf_width = src->right - src->left;
    surf_height = src->bottom - src->top;
    width = surf_width * (NV_COLOR_GET_BPP(surf->ColorFormat) / 8);
    height = surf_height;

    if( NVDDK_DISP_CAP_IS_SET( cont->caps, NvDdkDispCapBit_Bug_363177 ) )
    {
        /* prescaled size must be even for yuv surface formats, if vertical
         * scaling is active and this is window B.
         */
        if( ( surf->ColorFormat == NvColorFormat_VYUY ||
              surf->ColorFormat == NvColorFormat_Y8 ||
              surf->ColorFormat == NvColorFormat_U8 ) &&
            ( window == DC_HAL_WIN_B ) &&
            ( surf->Height < (NvU32)(( dest->bottom - dest->top )) ) )
        {
            height = ( height + 1 ) & ~1;
            width = ( width + 1 ) & ~1;
        }
    }

    val = NV_DRF_NUM( DC_WIN, B_PRESCALED_SIZE, B_H_PRESCALED_SIZE, width )
        | NV_DRF_NUM( DC_WIN, B_PRESCALED_SIZE, B_V_PRESCALED_SIZE, height );
    DC_WRITE( cont, DC_WIN_B_PRESCALED_SIZE_0, val );

    /* size */
    width = dest->right - dest->left;
    height = dest->bottom - dest->top;
    val = NV_DRF_NUM( DC_WIN, B_SIZE, B_H_SIZE, width )
        | NV_DRF_NUM( DC_WIN, B_SIZE, B_V_SIZE, height );
    DC_WRITE( cont, DC_WIN_B_SIZE_0, val );

    /* dda numbers are in 4.12 fixed point in hardware.
     * sw: surface width
     * dw: display width
     * the dda increment should be sw / dw
     * the initial should be 1/2 * ( sw + round / dw )
     * round is half dw
     */

    h_dda = ((surf_width << 12) + (width>>1)) / width;

    v_dda = ((surf_height << 12) + (height>>1)) / height;

    /* h_initial dda */
    val = NV_DRF_NUM( DC_WIN, B_H_INITIAL_DDA, B_H_INITIAL_DDA, h_dda >> 1 );
    DC_WRITE( cont, DC_WIN_B_H_INITIAL_DDA_0, val );

    /* v_initial dda */
    val = NV_DRF_NUM( DC_WIN, B_V_INITIAL_DDA, B_V_INITIAL_DDA, v_dda >> 1 );
    DC_WRITE( cont, DC_WIN_B_V_INITIAL_DDA_0, val );

    /* h_dda increment */
    val = NV_DRF_NUM( DC_WIN, B_DDA_INCREMENT, B_H_DDA_INCREMENT, h_dda );

    /* v_dda increment */
    val = NV_FLD_SET_DRF_NUM( DC_WIN, B_DDA_INCREMENT, B_V_DDA_INCREMENT,
        v_dda, val );
    DC_WRITE( cont, DC_WIN_B_DDA_INCREMENT_0, val );

    /* line stride (the surface pitch) */
    val = NV_DRF_NUM( DC_WIN, B_LINE_STRIDE, B_LINE_STRIDE, surf->Pitch );
    NV_DEBUG_CODE(
        if( surf->Layout == NvRmSurfaceLayout_Tiled )
        {
            /* line stride must be multiple of 16 */
            NV_ASSERT( (surf->Pitch & 15) == 0 );
        }
    );
    if( count > 1 )
    {
        NvRmSurface *s = surf + 1;
        val = NV_FLD_SET_DRF_NUM( DC_WIN, B_LINE_STRIDE, B_UV_LINE_STRIDE,
            s->Pitch, val );
    }
    else
    {
        val = NV_FLD_SET_DRF_NUM( DC_WIN, B_LINE_STRIDE, B_UV_LINE_STRIDE,
            surf->Pitch / 2, val );
    }
    DC_WRITE( cont, DC_WIN_B_LINE_STRIDE_0, val );

    /* buffer control */
    val = NV_DRF_DEF( DC_WIN, B_BUFFER_CONTROL, B_BUFFER_CONTROL, HOST );
    DC_WRITE( cont, DC_WIN_B_BUFFER_CONTROL_0, val );

    /* buffer address mode */
    if( surf->Layout == NvRmSurfaceLayout_Tiled )
    {
        val = NV_DRF_DEF( DC_WIN, B_BUFFER_ADDR_MODE, B_TILE_MODE, TILED )
            | NV_DRF_DEF( DC_WIN, B_BUFFER_ADDR_MODE, B_UV_TILE_MODE, TILED );
    }
    else
    {
        val = NV_DRF_DEF( DC_WIN, B_BUFFER_ADDR_MODE, B_TILE_MODE, LINEAR )
            | NV_DRF_DEF( DC_WIN, B_BUFFER_ADDR_MODE, B_UV_TILE_MODE, LINEAR );
    }
    DC_WRITE( cont, DC_WIN_B_BUFFER_ADDR_MODE_0, val );

    /* finally, write the surface start address */
    DcSetSurfaceAddress( cont, win, src, surf, count );

    /* save the rectangles */
    win->src_rect = *src;
    win->dst_rect = *dest;
    win->loc = *loc;
    DcHal_HwSetFiltering( controller, window, win->bHorizFiltering,
        win->bVertFiltering );
}

void
DcWaitStream( DcController *cont )
{
    if( cont->bNeedWaitStream )
    {
        NvRmChannelSyncPointWait( cont->hRm, cont->SyncPointId,
            cont->SyncPointShadow, cont->sem );
        cont->bNeedWaitStream = NV_FALSE;
    }
}

static void
DcCheckStream( DcController *cont, NvRmStream *stream )
{
    /* the display sync point is client managed -- the RM must not modifiy
     * its value.
     */
    NV_ASSERT( stream->SyncPointID == cont->SyncPointId );
    stream->ClientManaged = NV_TRUE;
    stream->SyncPointsUsed = cont->SyncPointShadow;
}

void DcHal_HwFlipWindowSurface( NvU32 controller, NvU32 window,
    NvRmSurface *surf, NvU32 count, NvRmStream *stream, NvRmFence *fence,
    NvU32 flags )
{
    DcController *cont;
    DcWindow *win;
    NvU32 val;
    NvU32 i;

    DC_CONTROLLER( controller, cont, return );
    DC_DIRTY( cont, window );

    win = &cont->windows[window];

    NV_ASSERT( win->nSurfs == count );

    NV_DEBUG_CODE(
        if( flags & NVDDK_DISP_FLIP_WAIT_FENCE )
        {
            NV_ASSERT( fence );
        }
    );

    /* this doesn't need begin/end/latch, etc., but it does need coordination
     * with the dsi ping mechanism, which is what the mutex below is for.
     */
    NvOsMutexLock( cont->frame_mutex );

    /* try to find the surface in the cache to avoid pin/unpin latencies */
    if( DcSurfaceCacheFind( win, surf, count ) == NV_FALSE )
    {
        for( i = 0; i < count; i++ )
        {
            NvRmSurface *s = surf + i;
            INCR_MEM_REF( s->hMem );
        }

        for( i = 0; i < count; i++ )
        {
            NvRmSurface *s = surf + i;
            win->hSurfs[i] = s->hMem;
        }
        NvRmMemPinMult( win->hSurfs, win->surf_addrs, win->nSurfs );

        DcSurfaceCacheAdd( win );
    }

    if( stream )
    {
        /**
         * prevent races with stream submits and writing to hardware directly.
         * direct writes lock the display hw mutex, so streams that logically
         * go first will not get a chance to execute.
         */
        cont->bNeedWaitStream = NV_TRUE;

        /* get some room in the stream buffer */
        DcCheckStream( cont, stream );
        NVRM_STREAM_BEGIN_WAIT( stream, cont->pb, DC_HAL_FLIP_WINDOW_WORDS,
            DC_HAL_FLIP_WINDOW_WAITS );

        cont->stream_fw = stream;

        /* push in a wait for given sync point */
        if( fence && (flags & NVDDK_DISP_FLIP_WAIT_FENCE) )
        {
            /* push in a wait for the given sync point id and value */
            NVRM_STREAM_PUSH_U( cont->pb,
                NVRM_CH_OPCODE_SET_CLASS( NV_HOST1X_CLASS_ID, 0, 0 ) );

            val = NV_DRF_NUM( NV_CLASS_HOST, WAIT_SYNCPT, INDX,
                    fence->SyncPointID )
                | NV_DRF_NUM( NV_CLASS_HOST, WAIT_SYNCPT, THRESH,
                    fence->Value );

            NVRM_STREAM_PUSH_U( cont->pb,
                NVRM_CH_OPCODE_NONINCR( NV_CLASS_HOST_WAIT_SYNCPT_0, 1 ) );
            NVRM_STREAM_PUSH_U( cont->pb, val );

            NVRM_STREAM_PUSH_WAIT_CHECK( stream, cont->pb, fence->SyncPointID,
                fence->Value );
        }

        /* set the current class to display */
        DcPushSetcl( cont );

        /* lock display */
        NVRM_STREAM_PUSH_U( cont->pb,
            NVRM_CH_OPCODE_ACQUIRE_MUTEX( cont->hw_mutex ) );

        /* set latchmux state to active if no-vsync-fence is specified.
         * this is to avoid flips going "back in time" and other sync point
         * management issues.
         */
        if( flags & NVDDK_DISP_FLIP_NO_VSYNC_FENCE )
        {
            val = NV_DRF_DEF( DC_CMD, STATE_ACCESS, READ_MUX, ACTIVE )
                | NV_DRF_DEF( DC_CMD, STATE_ACCESS, WRITE_MUX, ACTIVE );
            DC_WRITE_STREAM( cont, DC_CMD_STATE_ACCESS_0, val, cont->pb );
        }
    }
    else
    {
        /* set latchmux to active state */
        NV_ASSERT( !fence );

        HOST_CLOCK_ENABLE( cont );

        val = NV_DRF_DEF( DC_CMD, STATE_ACCESS, READ_MUX, ACTIVE )
            | NV_DRF_DEF( DC_CMD, STATE_ACCESS, WRITE_MUX, ACTIVE );
        DC_WRITE_DIRECT( cont, DC_CMD_STATE_ACCESS_0, val );
        DcLatch( cont, NV_TRUE );
        DC_FLUSH_DIRECT( cont );

        DC_BEGIN( cont );
    }

    DcSetWindow( cont, win->index );
    DcSetSurfaceAddress( cont, win, &win->src_rect, surf, count );

    if( stream )
    {
        if( flags & NVDDK_DISP_FLIP_NO_VSYNC_FENCE )
        {
            /* restore latchmux */
            val = NV_DRF_DEF( DC_CMD, STATE_ACCESS, READ_MUX, ASSEMBLY )
                | NV_DRF_DEF( DC_CMD, STATE_ACCESS, WRITE_MUX, ASSEMBLY );
            DC_WRITE_STREAM( cont, DC_CMD_STATE_ACCESS_0, val, cont->pb );
        }
        else
        {
            DcLatch( cont, NV_FALSE );
        }

        if( cont->mode.flags & NVDDK_DISP_MODE_FLAG_USE_TEARING_EFFECT )
        {
            val = NV_DRF_DEF( DC_CMD, STATE_CONTROL, NC_HOST_TRIG_ENABLE,
                    ENABLE )
                | NV_DRF_DEF( DC_CMD, STATE_CONTROL, GENERAL_ACT_REQ, ENABLE );
            DC_WRITE_STREAM( cont, DC_CMD_STATE_CONTROL_0, val, cont->pb );
            DcPushWaitFrame( cont );
        }
        else if( fence && !(flags & NVDDK_DISP_FLIP_NO_VSYNC_FENCE) )
        {
            /* must have frame_done in the command stream if a fence is given */
            DcPushWaitFrame( cont );
        }

        /* unlock display */
        NVRM_STREAM_PUSH_U( cont->pb,
            NVRM_CH_OPCODE_RELEASE_MUTEX( cont->hw_mutex ) );

        /* poke the sync point to ensure the pushbuffer made it through */
        DcPushImmediate( cont );

        NVRM_STREAM_END( stream, cont->pb );

        /* update client-managed state */
        stream->SyncPointsUsed = cont->SyncPointShadow;

        NvRmStreamFlush( stream, 0 );

        if( fence )
        {
            fence->SyncPointID = cont->SyncPointId;
            fence->Value = cont->SyncPointShadow;
        }

        cont->pb = 0;
        cont->stream_fw = NULL;
    }
    else
    {
        DC_END( cont );
        DC_FLUSH( cont );

        /* restore latchmux */
        val = NV_DRF_DEF( DC_CMD, STATE_ACCESS, READ_MUX, ASSEMBLY )
            | NV_DRF_DEF( DC_CMD, STATE_ACCESS, WRITE_MUX, ASSEMBLY );
        DC_WRITE_DIRECT( cont, DC_CMD_STATE_ACCESS_0, val );

        DcLatch( cont, NV_TRUE );
        DC_FLUSH_DIRECT( cont );

        HOST_CLOCK_DISABLE( cont );
    }

    NvOsMutexUnlock( cont->frame_mutex );
}

void
DcHal_HwSetDepth( NvU32 controller, NvU32 window, NvU32 depth )
{
    DcController *cont;
    DcWindow *win;

    DC_CONTROLLER( controller, cont, return );

    win = &cont->windows[window];
    win->depth = depth;

    cont->update_blend = NV_TRUE;
}

void
DcHal_HwSetFiltering( NvU32 controller, NvU32 window, NvBool h_filter,
    NvBool v_filter )
{
    DcController *cont;
    DcWindow *win;
    NvU32 val;

    DC_CONTROLLER( controller, cont, return );
    DC_SAFE( cont, return );
    DC_DIRTY( cont, window );

    /* set hardware window */
    DcSetWindow( cont, window );

    // FIXME: check window filter caps

    win = &cont->windows[window];
    win->bHorizFiltering = h_filter;
    win->bVertFiltering = v_filter;

    /* filter registers are already initialized */
    val = win->win_options;
    if( h_filter || v_filter )
    {
        /* check rectangles */
        if( ((win->src_rect.right - win->src_rect.left) !=
             (win->dst_rect.right - win->dst_rect.left)) &&
            h_filter )
        {
            val = NV_FLD_SET_DRF_DEF( DC_WIN, B_WIN_OPTIONS, B_H_FILTER_ENABLE,
                ENABLE, val );
        }
        else
        {
            val = NV_FLD_SET_DRF_DEF( DC_WIN, B_WIN_OPTIONS, B_H_FILTER_ENABLE,
                DISABLE, val );
        }

        if( ((win->src_rect.bottom - win->src_rect.top) !=
             (win->dst_rect.bottom - win->dst_rect.top)) &&
            v_filter )
        {
            val = NV_FLD_SET_DRF_DEF( DC_WIN, B_WIN_OPTIONS, B_V_FILTER_ENABLE,
                ENABLE, val );
        }
        else
        {
            val = NV_FLD_SET_DRF_DEF( DC_WIN, B_WIN_OPTIONS, B_V_FILTER_ENABLE,
                DISABLE, val );
        }
    }
    else
    {
        val = NV_FLD_SET_DRF_DEF( DC_WIN, B_WIN_OPTIONS, B_H_FILTER_ENABLE,
            DISABLE, val );
        val = NV_FLD_SET_DRF_DEF( DC_WIN, B_WIN_OPTIONS, B_V_FILTER_ENABLE,
            DISABLE, val );
    }
    win->win_options = val;

    DC_WRITE( cont, DC_WIN_B_WIN_OPTIONS_0, val );
}

void
DcHal_HwSetVibrance( NvU32 controller, NvU32 window, NvU32 vib )
{
    DcController *cont;
    DcWindow *win;
    NvU32 val;
    NvU32 vr, vg, vb;

    DC_CONTROLLER( controller, cont, return );
    DC_SAFE( cont, return );
    DC_DIRTY( cont, window );

    /* set hardware window */
    DcSetWindow( cont, window );

    win = &cont->windows[window];

    vr = vib & 0x3;
    vg = ((vib & (0x3 << 8 )) >> 8) & 0x3;
    vb = ((vib & (0x3 << 16)) >> 16) & 0x3;

    val = NV_DRF_NUM( DC_WIN, B_DV_CONTROL, B_DV_CONTROL_R, vr )
        | NV_DRF_NUM( DC_WIN, B_DV_CONTROL, B_DV_CONTROL_G, vg )
        | NV_DRF_NUM( DC_WIN, B_DV_CONTROL, B_DV_CONTROL_B, vb );
    DC_WRITE( cont, DC_WIN_B_DV_CONTROL_0, val );

    val = win->win_options;
    if( vib )
    {
        val = NV_FLD_SET_DRF_DEF( DC_WIN, B_WIN_OPTIONS, B_DV_ENABLE,
            ENABLE, val );
    }
    else
    {
        val = NV_FLD_SET_DRF_DEF( DC_WIN, B_WIN_OPTIONS, B_DV_ENABLE,
            DISABLE, val );
    }
    win->win_options = val;

    DC_WRITE( cont, DC_WIN_B_WIN_OPTIONS_0, val );
}

void
DcHal_HwSetScaleNicely( NvU32 controller, NvU32 window, NvBool scale )
{
    DcController *cont;

    DC_CONTROLLER( controller, cont, return );
    DC_SAFE( cont, return );

    DcSetWindow( cont, window );
    DC_DIRTY( cont, window );

    /* make sure window header is set before writing filter registers */
    if( !NVDDK_DISP_CAP_IS_SET(
        cont->caps, NvDdkDispCapBit_IndirectRegs ) )
    {
        DC_FLUSH_DIRECT( cont );
    }

    if( scale )
    {
        DcExecTable( cont, NV_ARRAY_SIZE(DcNicestFilterTable_Regs),
            DcNicestFilterTable_Regs, DcNicestFilterTable_Vals );
    }
    else
    {
        DcExecTable( cont, NV_ARRAY_SIZE(DcLinearFilterTable_Regs),
            DcLinearFilterTable_Regs, DcLinearFilterTable_Vals );
    }
}

void
DcHal_HwSetBlend( NvU32 controller, NvU32 window,
    NvDdkDispBlendType blend )
{
    DcController *cont;
    DcWindow *win;

    DC_CONTROLLER( controller, cont, return );

    win = &cont->windows[window];
    win->blend_op = blend;

    cont->update_blend = NV_TRUE;
}

void
DcHal_HwSetAlphaOp( NvU32 controller, NvU32 window,
    NvDdkDispAlphaOperation op, NvDdkDispAlphaBlendDirection dir )
{
    DcController *cont;
    DcWindow *win;

    DC_CONTROLLER( controller, cont, return );

    if( dir == NvDdkDispAlphaBlendDirection_Default )
    {
        dir = NvDdkDispAlphaBlendDirection_Destination;
    }

    win = &cont->windows[window];
    win->alpha_op = op;
    win->alpha_dir = dir;

    cont->update_blend = NV_TRUE;
}

void
DcHal_HwSetAlpha( NvU32 controller, NvU32 window, NvU8 alpha )
{
    DcController *cont;
    DcWindow *win;

    DC_CONTROLLER( controller, cont, return );

    win = &cont->windows[window];
    win->alpha = alpha;

    cont->update_blend = NV_TRUE;
}

void
DcHal_HwSetCsc( NvU32 controller, NvU32 window,
    NvDdkDispColorSpaceCoef *csc )
{
    DcController *cont;
    NvU32 val;

    DC_CONTROLLER( controller, cont, return );
    DC_SAFE( cont, return );
    DC_DIRTY( cont, window );

    DcSetWindow( cont, window );

    /* csc is enabled/disabled in DcHal_HwSetWindowSurface */

    val = NV_DRF_NUM( DC_WINC, B_CSC_YOF, B_CSC_YOF, csc->K >> 16 );
    DC_WRITE( cont, DC_WINC_B_CSC_YOF_0, val );

    val = NV_DRF_NUM( DC_WINC, B_CSC_KYRGB, B_CSC_KYRGB, csc->C11 >> 8 );
    DC_WRITE( cont, DC_WINC_B_CSC_KYRGB_0, val );

    val = NV_DRF_NUM( DC_WINC, B_CSC_KUR, B_CSC_KUR, csc->C12 >> 8 );
    DC_WRITE( cont, DC_WINC_B_CSC_KUR_0, val );

    val = NV_DRF_NUM( DC_WINC, B_CSC_KVR, B_CSC_KVR, csc->C13 >> 8 );
    DC_WRITE( cont, DC_WINC_B_CSC_KVR_0, val );

    val = NV_DRF_NUM( DC_WINC, B_CSC_KUG, B_CSC_KUG, csc->C22 >> 8 );
    DC_WRITE( cont, DC_WINC_B_CSC_KUG_0, val );

    val = NV_DRF_NUM( DC_WINC, B_CSC_KVG, B_CSC_KVG, csc->C23 >> 8);
    DC_WRITE( cont, DC_WINC_B_CSC_KVG_0, val );

    val = NV_DRF_NUM( DC_WINC, B_CSC_KUB, B_CSC_KUB, csc->C32 >> 8 );
    DC_WRITE( cont, DC_WINC_B_CSC_KUB_0, val );

    val = NV_DRF_NUM( DC_WINC, B_CSC_KVB, B_CSC_KVB, csc->C33 >> 8 );
    DC_WRITE( cont, DC_WINC_B_CSC_KVB_0, val );
}

void
DcHal_HwSetColorKey( NvU32 controller, NvU32 window, NvU32 upper, NvU32 lower )
{
    DcController *cont;
    DcWindow *win;

    DC_CONTROLLER( controller, cont, return );

    win = &cont->windows[window];
    win->colorkey_lower = lower;
    win->colorkey_upper = upper;

    cont->update_blend = NV_TRUE;
}

void
DcHal_HwSetColorCorrection( NvU32 controller, NvU32 brightness,
    NvU32 gamma_red, NvU32 gamma_green, NvU32 gamma_blue,
    NvU32 scale_red, NvU32 scale_green, NvU32 scale_blue,
    NvU32 contrast )
{
    DcController *cont;
    DcWindow *win;
    NvU32 val;
    NvBool bEnable = NV_TRUE;
    NvS32 i, j;
    NvU32 gamma[3];
    NvU32 scale[3];

    DC_CONTROLLER( controller, cont, return );
    DC_SAFE( cont, return );

    if( gamma_red == 0x00010000 && gamma_green == 0x00010000 &&
        gamma_blue == 0x00010000 && scale_red == 0x00010000 &&
        scale_green == 0x00010000 && scale_blue == 0x00010000 &&
        contrast == 0x00010000 && brightness == 0 )
    {
        bEnable = NV_FALSE;
    }

    for( i = 0; i < DC_HAL_WINDOWS; i++ )
    {
        DcSetWindow( cont, i );
        win = &cont->windows[i];

        DC_DIRTY( cont, i );

        val = win->win_options;

        if( !bEnable )
        {
            /* disable csc */
            val = NV_FLD_SET_DRF_DEF( DC_WIN, B_WIN_OPTIONS, B_CP_ENABLE,
                DISABLE, val );
        }
        else
        {
            val = NV_FLD_SET_DRF_DEF( DC_WIN, B_WIN_OPTIONS, B_CP_ENABLE,
                ENABLE, val );
        }

        win->win_options = val;

        DC_WRITE( cont, DC_WIN_B_WIN_OPTIONS_0, val );
    }

    if( !bEnable )
    {
        return;
    }

    gamma[0] = gamma_red;
    gamma[1] = gamma_green;
    gamma[2] = gamma_blue;
    scale[0] = scale_red;
    scale[1] = scale_green;
    scale[2] = scale_blue;


    /* update color palette */
    for( i = 0; i < 256; i++ )
    {
        NvSFx color[3]; // r, g, b;

        for( j = 0; j < 3; j++ )
        {
            // This can be simplified a lot, especially when brightness=0 and
            // contrast =1. because:
            // ((X * ((color/255)^2.2))^(1/2.2)) * 255 = X^(1/2.2) * color
            // = Xg * color where Xg is X^(1/2.2). Xg is the scale factor we get
            // from display odm.
            // Breakup the computation for now so future change will be easier.
            // Also since (a*b)^n = (a^n) * (b^n), we can do scaling
            // either before or after gamma. But ODM supplies scaling in linear
            // space.

            // first scale value down to [0, 1]:
            color[j] = i << 16;
            color[j] /= 255;

            /* apply brightness */
            color[j] = NV_SFX_MIN(
                NV_SFX_MAX( 0, color[j] + (NvSFx)(brightness/255) ),
                (1 << 16) );

            /* apply contrast */
            // shift to [-0.5, +0.5]:
            color[j] -= (1 << 15); // subtract 0.5
            // apply contrast factor:
            color[j] = NV_SFX_MUL( color[j], contrast );
            // shift back to [0, 1] and clamp:
            color[j] += (1 << 15); // add 0.5
            color[j] = NV_SFX_MIN( NV_SFX_MAX( 0, color[j] ), (1 << 16) );

            /* apply scale */
            color[j] = NV_SFX_MUL( color[j], scale[j] );
            // clamp to [0, 1]
            color[j] = NV_SFX_MIN( color[j], 1 << 16 );

            /* apply custom gamma */
            if( (color[j] != 0 ) &&
                (gamma[j] != 0x00010000) )
            {
                color[j] = NvSFxPow( color[j], gamma[j] );
                // clamp to [0, 1] again:
                color[j] = NV_SFX_MAX( color[j], 0 );
            }

            /* convert back to [0, 255] */
            color[j] = 255 * color[j];

            /* add 0.5 before truncate to round up */
            color[j] += (1 << 15);
            color[j] = NV_SFX_TRUNCATE_TO_WHOLE(color[j]);

            /* extra clamp on overflow */
            color[j] = NV_SFX_MIN( color[j], 255);
        }
        val = NV_DRF_NUM( DC_WINC, B_COLOR_PALETTE, B_COLOR_PALETTE_R,
                color[0] )
            | NV_DRF_NUM( DC_WINC, B_COLOR_PALETTE, B_COLOR_PALETTE_G,
                color[1] )
            | NV_DRF_NUM( DC_WINC, B_COLOR_PALETTE, B_COLOR_PALETTE_B,
                color[2] );

        /* update all windows */
        for( j = 0; j < DC_HAL_WINDOWS; j++ )
        {
            DcSetWindow( cont, j );
            DC_WRITE( cont, DC_WINC_B_COLOR_PALETTE_0 + i, val );
        }
    }
}

void
DcHal_HwSetBacklight( NvU32 controller, NvU8 intensity )
{
    DcController *cont;

    DC_CONTROLLER( controller, cont, return );
    DC_SAFE( cont, return );

    switch( cont->PwmOutputPin ) {
    case DcPwmOutputPin_Unspecified:
        break;
    case DcPwmOutputPin_LM1_PM1:
        DcSetBacklight(cont, DcSetBacklightConfigPin_LM1_PM1, intensity);
        break;
    case DcPwmOutputPin_LDC_PM1:
        DcSetBacklight(cont, DcSetBacklightConfigPin_LDC_PM1, intensity);
        break;
    case DcPwmOutputPin_LPW0_PM0:
        DcSetBacklight(cont, DcSetBacklightConfigPin_LPW0_PM0, intensity);
        break;
    case DcPwmOutputPin_LPW1_PM1:
        DcSetBacklight(cont, DcSetBacklightConfigPin_LPW1_PM1, intensity);
        break;
    case DcPwmOutputPin_LPW2_PM0:
        DcSetBacklight(cont, DcSetBacklightConfigPin_LPW2_PM0, intensity);
        break;
    case DcPwmOutputPin_LM0_PM0:
        DcSetBacklight(cont, DcSetBacklightConfigPin_LM0_PM0, intensity);
        break;
    default:
        NV_ASSERT( !"bad pwm output pin" );
        break;
    }
}

void
DcSetBacklight( DcController *cont, DcSetBacklightConfigPin conf,
    NvU8 intensity )
{
    NvU32 val = 0;
    NvBool pm1 = NV_FALSE;

    switch( conf ) {
    case DcSetBacklightConfigPin_LPW2_PM0:
        val = cont->output_select3;
        val = NV_FLD_SET_DRF_NUM(DC, COM_PIN_OUTPUT_SELECT3,
                LPW2_OUTPUT_SELECT, 3, val );
        cont->output_select3 = val;
        DC_WRITE( cont, DC_COM_PIN_OUTPUT_SELECT3_0, val );

        break;
    case DcSetBacklightConfigPin_LPW0_PM0:
        val = cont->output_select3;
        val = NV_FLD_SET_DRF_NUM(DC, COM_PIN_OUTPUT_SELECT3,
            LPW0_OUTPUT_SELECT, 3, val );
        cont->output_select3 = val;
        DC_WRITE( cont, DC_COM_PIN_OUTPUT_SELECT3_0, val );

        break;
    case DcSetBacklightConfigPin_LM0_PM0:
        val = NV_DRF_NUM(DC, COM_PIN_OUTPUT_SELECT5, LM0_OUTPUT_SELECT,3);
        if( cont->pinmap == NvOdmDispPinMap_Single_Rgb24_Spi5 )
        {
            val = NV_FLD_SET_DRF_NUM( DC, COM_PIN_OUTPUT_SELECT5,
                LPP_OUTPUT_SELECT, 2, val);
            val = NV_FLD_SET_DRF_NUM( DC, COM_PIN_OUTPUT_SELECT5,
                LDI_OUTPUT_SELECT, 2, val);
        }
        DC_WRITE( cont, DC_COM_PIN_OUTPUT_SELECT5_0, val );
        break;
    case DcSetBacklightConfigPin_LHP0_PM0:
        val = NV_DRF_NUM(DC, COM_PIN_OUTPUT_SELECT4, LHP0_OUTPUT_SELECT,3);
        DC_WRITE( cont, DC_COM_PIN_OUTPUT_SELECT4_0, val );
        break;
    case DcSetBacklightConfigPin_LHP2_PM0:
        val = NV_DRF_NUM(DC, COM_PIN_OUTPUT_SELECT4, LHP2_OUTPUT_SELECT,3);
        DC_WRITE( cont, DC_COM_PIN_OUTPUT_SELECT4_0, val );
        break;
    case DcSetBacklightConfigPin_LVP0_PM0:
        val = NV_DRF_NUM(DC, COM_PIN_OUTPUT_SELECT4, LVP0_OUTPUT_SELECT,3);
        DC_WRITE( cont, DC_COM_PIN_OUTPUT_SELECT4_0, val );
        break;
    case DcSetBacklightConfigPin_LDI_PM0:
        val = NV_DRF_NUM(DC, COM_PIN_OUTPUT_SELECT5, LDI_OUTPUT_SELECT,3);
        DC_WRITE( cont, DC_COM_PIN_OUTPUT_SELECT5_0, val );
        break;
    case DcSetBacklightConfigPin_LHS_PM0:
        val = cont->output_select3;
        val = NV_FLD_SET_DRF_NUM( DC, COM_PIN_OUTPUT_SELECT3,
            LHS_OUTPUT_SELECT, 3, val );
        cont->output_select3 = val;
        DC_WRITE( cont, DC_COM_PIN_OUTPUT_SELECT3_0, val );
        break;
    case DcSetBacklightConfigPin_LSCK_PM0:
        val = NV_DRF_NUM(DC, COM_PIN_OUTPUT_SELECT6, LSCK_OUTPUT_SELECT,3);
        DC_WRITE( cont, DC_COM_PIN_OUTPUT_SELECT6_0, val );
        break;
    case DcSetBacklightConfigPin_LCS_PM0:
        val = NV_DRF_NUM(DC, COM_PIN_OUTPUT_SELECT6, LCSN_OUTPUT_SELECT,3);
        DC_WRITE( cont, DC_COM_PIN_OUTPUT_SELECT6_0, val );
        break;
    case DcSetBacklightConfigPin_LSPI_PM0:
        val = NV_DRF_NUM(DC, COM_PIN_OUTPUT_SELECT6, LSPI_OUTPUT_SELECT,3);
        DC_WRITE( cont, DC_COM_PIN_OUTPUT_SELECT6_0, val );
        break;
    case DcSetBacklightConfigPin_LM1_PM1:
        val = NV_DRF_NUM(DC, COM_PIN_OUTPUT_SELECT5, LM1_OUTPUT_SELECT,3);
        if( cont->pinmap == NvOdmDispPinMap_Single_Rgb24_Spi5 )
        {
            val = NV_FLD_SET_DRF_NUM( DC, COM_PIN_OUTPUT_SELECT5,
                LPP_OUTPUT_SELECT, 2, val );
            val = NV_FLD_SET_DRF_NUM( DC, COM_PIN_OUTPUT_SELECT5,
                LDI_OUTPUT_SELECT, 2, val );
        }
        DC_WRITE( cont, DC_COM_PIN_OUTPUT_SELECT5_0, val );
        pm1 = NV_TRUE;
        break;
    case DcSetBacklightConfigPin_LPW1_PM1:
        val = cont->output_select3;
        val = NV_FLD_SET_DRF_NUM(DC, COM_PIN_OUTPUT_SELECT3,
            LPW1_OUTPUT_SELECT, 3, val );
        cont->output_select3 = val;
        DC_WRITE( cont, DC_COM_PIN_OUTPUT_SELECT3_0, val );
        pm1 = NV_TRUE;
        break;
    case DcSetBacklightConfigPin_LVP1_PM1:
        val = NV_DRF_NUM(DC, COM_PIN_OUTPUT_SELECT4, LVP1_OUTPUT_SELECT,3);
        DC_WRITE( cont, DC_COM_PIN_OUTPUT_SELECT4_0, val );
        pm1 = NV_TRUE;
        break;
    case DcSetBacklightConfigPin_LHP1_PM1:
        val = NV_DRF_NUM(DC, COM_PIN_OUTPUT_SELECT4, LHP1_OUTPUT_SELECT,3);
        DC_WRITE( cont, DC_COM_PIN_OUTPUT_SELECT4_0, val );
        pm1 = NV_TRUE;
        break;
    case DcSetBacklightConfigPin_LPP_PM1:
        val = NV_DRF_NUM(DC, COM_PIN_OUTPUT_SELECT5, LPP_OUTPUT_SELECT,3);
        DC_WRITE( cont, DC_COM_PIN_OUTPUT_SELECT5_0, val );
        pm1 = NV_TRUE;
        break;
    case DcSetBacklightConfigPin_LVS_PM1:
        val = cont->output_select3;
        val = NV_FLD_SET_DRF_NUM(DC, COM_PIN_OUTPUT_SELECT3,
            LVS_OUTPUT_SELECT, 3, val );
        cont->output_select3 = val;
        DC_WRITE( cont, DC_COM_PIN_OUTPUT_SELECT3_0, val );
        pm1 = NV_TRUE;
        break;
    case DcSetBacklightConfigPin_LSDA_PM1:
        val = NV_DRF_NUM(DC, COM_PIN_OUTPUT_SELECT6, LSDA_OUTPUT_SELECT,3);
        DC_WRITE( cont, DC_COM_PIN_OUTPUT_SELECT6_0, val );
        pm1 = NV_TRUE;
        break;
    case DcSetBacklightConfigPin_LDC_PM1:
        val = NV_DRF_NUM(DC, COM_PIN_OUTPUT_SELECT6, LDC_OUTPUT_SELECT,3);
        if( (cont->pinmap == NvOdmDispPinMap_Single_Rgb18) ||
            (cont->pinmap == NvOdmDispPinMap_Single_Rgb24_Spi5) )
        {
            val = NV_FLD_SET_DRF_NUM( DC, COM_PIN_OUTPUT_SELECT6,
                LSPI_OUTPUT_SELECT, 2, val );
        }
        DC_WRITE( cont, DC_COM_PIN_OUTPUT_SELECT6_0, val );
        pm1 = NV_TRUE;
        break;
    default:
        NV_ASSERT( !"bad config pin" );
        break;
    }

    if( NVDDK_DISP_CAP_IS_SET( cont->caps, NvDdkDispCapBit_Bug_722394) )
    {
        // period = 128 clock cycles, clock select from line clock
        val = NV_DRF_NUM( DC, COM_PM1_CONTROL, PM1_PERIOD, 0x1F )
            | NV_DRF_NUM( DC, COM_PM1_CONTROL, PM1_CLOCK_DIVIDER, 3 )
            | NV_DRF_NUM( DC, COM_PM1_CONTROL, PM1_CLOCK_SELECT, 2 );

        // map API intensity range from (0~255) to hw intensity range (0~128)
        intensity = (intensity * 128) / 255;
    }
    else
    {
        // period = 256 clock cycles, clock select from line clock
        val = NV_DRF_NUM( DC, COM_PM1_CONTROL, PM1_PERIOD, 0x3F )
            | NV_DRF_NUM( DC, COM_PM1_CONTROL, PM1_CLOCK_DIVIDER, 1 )
            | NV_DRF_NUM( DC, COM_PM1_CONTROL, PM1_CLOCK_SELECT, 2 );
    }

    if( pm1 )
    {
        DC_WRITE( cont, DC_COM_PM1_CONTROL_0, val);
    }
    else
    {
        DC_WRITE( cont, DC_COM_PM0_CONTROL_0, val);
    }

    // duty cycle
    val = NV_DRF_NUM( DC, COM_PM1_DUTY_CYCLE, PM1_DUTY_CYCLE, intensity );

    if (pm1)
    {
        DC_WRITE( cont, DC_COM_PM1_DUTY_CYCLE_0, val );
    }
    else
    {
        DC_WRITE( cont, DC_COM_PM0_DUTY_CYCLE_0, val );
    }
}

void
DcHal_HwSetBackgroundColor( NvU32 controller, NvU32 color )
{
    DcController *cont;
    NvU32 val;

    DC_CONTROLLER( controller, cont, return );
    DC_SAFE( cont, return );

    val = NV_DRF_NUM( DC_DISP, BORDER_COLOR, BORDER_COLOR_B,
            ( color & 0xff ) )
        | NV_DRF_NUM( DC_DISP, BORDER_COLOR, BORDER_COLOR_G,
            ( ( color & ( 0xff << 8 ) ) >> 8 ) )
        | NV_DRF_NUM( DC_DISP, BORDER_COLOR, BORDER_COLOR_R,
            ( ( color & ( 0xff << 16 ) ) >> 16 ) );
    DC_WRITE( cont, DC_DISP_BORDER_COLOR_0, val );
}

void
DcHal_HwSetCursor( NvU32 controller, NvRmMemHandle mem, NvSize *size,
    NvU32 fgColor, NvU32 bgColor )
{
    DcController *cont;
    NvU32 val;
    NvU32 addr;

    DC_CONTROLLER( controller, cont, return );
    DC_SAFE( cont, return );

    val = NV_DRF_NUM( DC_DISP, CURSOR_FOREGROUND, CURSOR_FOREGROUND_R,
              (fgColor & 0xff0000) >> 16 )
        | NV_DRF_NUM( DC_DISP, CURSOR_FOREGROUND, CURSOR_FOREGROUND_G,
              (fgColor & 0xff00) >> 8 )
        | NV_DRF_NUM( DC_DISP, CURSOR_FOREGROUND, CURSOR_FOREGROUND_B,
              fgColor & 0xff );
    DC_WRITE( cont, DC_DISP_CURSOR_FOREGROUND_0, val );

    val = NV_DRF_NUM( DC_DISP, CURSOR_BACKGROUND, CURSOR_BACKGROUND_R,
              (bgColor & 0xff0000) >> 16 )
        | NV_DRF_NUM( DC_DISP, CURSOR_BACKGROUND, CURSOR_BACKGROUND_G,
              (bgColor & 0xff00) >> 8 )
        | NV_DRF_NUM( DC_DISP, CURSOR_BACKGROUND, CURSOR_BACKGROUND_B,
              bgColor & 0xff );
    DC_WRITE( cont, DC_DISP_CURSOR_BACKGROUND_0, val );

    addr = NvRmMemPin( mem );
    val = NV_DRF_NUM( DC_DISP, CURSOR_START_ADDR, CURSOR_START_ADDR,
            addr >> 10 )
        | NV_DRF_DEF( DC_DISP, CURSOR_START_ADDR, CURSOR_CLIPPING,
            DISPLAY );

    if( size->height == 64 )
    {
        val = NV_FLD_SET_DRF_DEF( DC_DISP, CURSOR_START_ADDR, CURSOR_SIZE,
            C64X64, val );
    }
    else
    {
        val = NV_FLD_SET_DRF_DEF( DC_DISP, CURSOR_START_ADDR, CURSOR_SIZE,
            C32X32, val );
    }

    DC_WRITE( cont, DC_DISP_CURSOR_START_ADDR_0, val );
}

void
DcHal_HwSetCursorPosition( NvU32 controller, NvPoint *pt )
{
    DcController *cont;
    NvU32 val;

    DC_CONTROLLER( controller, cont, return );

    /* this does not need a HwBegin()/HwEnd() pair, but DC_BEGIN/DC_END are
     * still necessary. need to check if HwBegin() has been called though.
     */

    if( !cont->bSafe )
    {
        DC_BEGIN( cont );
    }

    if( pt )
    {
        // FIXME: check max resolution, hot spot
        val = NV_DRF_NUM( DC_DISP, CURSOR_POSITION, H_CURSOR_POSITION, pt->x )
            | NV_DRF_NUM( DC_DISP, CURSOR_POSITION, V_CURSOR_POSITION, pt->y );
        DC_WRITE( cont, DC_DISP_CURSOR_POSITION_0, val );

        DcEnable( cont, DC_HAL_CURSOR, NV_TRUE );
    }
    else
    {
        DcEnable( cont, DC_HAL_CURSOR, NV_FALSE );
    }

    if( !cont->bSafe )
    {
        DcLatch( cont, NV_FALSE );
        DC_END( cont );
        DC_FLUSH( cont );
    }
}

/* this does not need begin/end */
NvBool
DcHal_HwContentProtection( NvU32 controller, NvDdkDispContentProtection
    protection, void *info, NvDdkDispTvoHal *TvHal,
    NvDdkDispHdmiHal *HdmiHal )
{
    DcController *cont;
    NvBool b;

    DC_CONTROLLER( controller, cont, return NV_FALSE );

    switch( protection ) {
    case NvDdkDispContentProtection_None:
        if( cont->bHdcp )
        {
            HdmiHal->HwHdcpOp( controller, (NvDdkDispHdcpContext *)info,
                NV_FALSE );
            cont->bHdcp = NV_FALSE;
        }
        if( cont->bMacrovision || cont->bCgmsa )
        {
            TvHal->HwTvMacrovisionSetContext( controller, 0 );
            // this disables all analog content protection
            TvHal->HwTvMacrovisionEnable( controller, NV_FALSE );
            cont->bMacrovision = NV_FALSE;
            cont->bCgmsa = NV_FALSE;
        }
        break;
    case NvDdkDispContentProtection_HDCP:
    {
        NvDdkDispHdcpContext *context;

        context = (NvDdkDispHdcpContext *)info;

        b = HdmiHal->HwHdcpOp( controller, context, NV_TRUE );
        if( !b )
        {
            context->State = NvDdkDispHdcpState_Unauthenticated;
            cont->bHdcp = NV_FALSE;
            goto fail;
        }

        cont->bHdcp = NV_TRUE;
        break;
    }
    case NvDdkDispContentProtection_Macrovision:
        if( !cont->bMacrovision )
        {
            NvDdkDispMacrovisionContext *context =
                (NvDdkDispMacrovisionContext *)info;

            TvHal->HwTvMacrovisionSetContext( controller, context );

            b = TvHal->HwTvMacrovisionEnable( controller, NV_TRUE );
            if( !b )
            {
                goto fail;
            }

            cont->bMacrovision = NV_TRUE;
        }
        break;
    case NvDdkDispContentProtection_CGMSA:
    {
        NvDdkDispCGMSAContext *context;
        context = (NvDdkDispCGMSAContext *)info;
        if( context->LineSelect & 1 )
        {
            TvHal->HwTvMacrovisionSetCGMSA_20( controller,
                context->Payload );
        }
        if( (context->LineSelect >> 1) & 1 )
        {
            TvHal->HwTvMacrovisionSetCGMSA_21( controller,
                context->Payload );
        }
        break;
    }
    default:
        goto fail;
    }

    b = NV_TRUE;
    goto clean;

fail:
    b = NV_FALSE;

clean:
    return b;
}

void
DcHal_SetErrorTrigger( NvU32 controller, NvU32 bitfield,
    NvOsSemaphoreHandle hSem, NvU32 flags )
{
    DcController *cont;
    NvU32 tmp;

    DC_CONTROLLER( controller, cont, return );

    NvOsIntrMutexLock( cont->IntrMutex );
    tmp = cont->InterruptActiveErrors;
    NvOsIntrMutexUnlock( cont->IntrMutex );

    /* check for an already-pending error */
    if( bitfield & tmp )
    {
        NvOsSemaphoreSignal( hSem );
    }
    else
    {
        cont->InterruptBits = bitfield;
        cont->InterruptSem = hSem;
        cont->bErrorTriggerEnable = NV_TRUE;

        DcSetInterrupts( cont );
    }
}

NvU32
DcHal_HwGetLastActiveError( NvU32 controller )
{
    DcController *cont;
    NvU32 val;

    DC_CONTROLLER( controller, cont, return 0 );

    NvOsIntrMutexLock( cont->IntrMutex );

    val = cont->InterruptActiveErrors;
    cont->InterruptActiveErrors = 0;
    cont->InterruptSem = 0;
    cont->bErrorTriggerEnable = NV_FALSE;

    NvOsIntrMutexUnlock( cont->IntrMutex );

    return val;
}

void
DcHal_HwSetErrorSem( NvU32 controller, NvOsSemaphoreHandle hSem )
{
    DcController *cont;
    NvU32 tmp;

    DC_CONTROLLER( controller, cont, return );
    NV_ASSERT( hSem );

    NvOsIntrMutexLock( cont->IntrMutex );
    tmp = cont->InternalError;
    NvOsIntrMutexUnlock( cont->IntrMutex );

    if( tmp )
    {
        NvOsSemaphoreSignal( hSem );
    }
    else
    {
        cont->InternalSem = hSem;
        cont->bInternalErrorEnable = NV_TRUE;

        DcSetInterrupts( cont );
    }
}

NvU32 DcHal_HwGetLastError( NvU32 controller )
{
    DcController *cont;
    NvU32 reg, val;
    NvU32 e;

    DC_CONTROLLER( controller, cont, return 0 );

    NvOsIntrMutexLock( cont->IntrMutex );

    e = cont->InternalError;
    cont->InternalError = 0;
    cont->InternalSem = 0;
    cont->bInternalErrorEnable = NV_FALSE;

    NvOsIntrMutexUnlock( cont->IntrMutex );

    if( !e )
    {
        /* read from hardware */
        e = DcGetInterruptType( cont );
    }

    /* clear interrupts */
    reg = DC_CMD_INT_STATUS_0 * 4;
    val = 0xFFFFFFFF;
    DcRegWr( cont, 1, &reg, &val );

    return e;
}

void
DcRegWr( DcController *cont, NvU32 num, const NvU32 *offsets,
    const NvU32 *values )
{
    NvU32 i;
    NvU32 *addr;

    HOST_CLOCK_ENABLE( cont );

    for( i = 0; i < num; i++ )
    {
        addr = (void *)((NvUPtr)cont->registers + offsets[i] );
        NV_WRITE32( addr, values[i] );
    }

    HOST_CLOCK_DISABLE( cont );
}

void
DcRegRd( DcController *cont, NvU32 num, const NvU32 *offsets,
    NvU32 *values )
{
    NvU32 i;
    NvU32 *addr;

    HOST_CLOCK_ENABLE( cont );

    for( i = 0; i < num; i++ )
    {
        addr = (void *)((NvUPtr)cont->registers + offsets[i] );
        values[i] = NV_READ32( addr );
    }

    HOST_CLOCK_DISABLE( cont );
}

void
DcExecTable( DcController *cont, NvU32 num, NvU32 *regs, NvU32 *vals )
{
    NvU32 i;

    NV_ASSERT( cont );
    NV_ASSERT( num );
    NV_ASSERT( regs );
    NV_ASSERT( vals );

#if NV_ENABLE_DEBUG_PRINTS && DC_DUMP_REGS
    for( i = 0; i < num; i++ )
    {
        NV_DEBUG_PRINTF(( "disp: %.8x %.8x\n", regs[i]/4, vals[i] ));
    }
#endif

    if( NVDDK_DISP_CAP_IS_SET( cont->caps, NvDdkDispCapBit_IndirectRegs ) )
    {
        for( i = 0; i < num; i++ )
        {
            DC_WRITE_STREAM( cont, regs[i] / 4, vals[i], cont->pb );
        }
    }
    else
    {
        /* dump the data to the hardware */
        DcRegWr( cont, num, regs, vals );
    }
}

void
DcEnable( DcController *cont, NvU32 window, NvBool enable )
{
    NvBool bUpdateBlend = NV_TRUE;

    if( enable )
    {
        NvU32 mem_high_priority;
        NvU32 mem_high_priority_timer;

        cont->win_enable |= DC_WIN_MASK( window );

        mem_high_priority = cont->mem_high_priority;
        mem_high_priority_timer = cont->mem_high_priority_timer;

        /* configure the memory controller priority */
        switch( window ) {
        case DC_HAL_WIN_A:
            mem_high_priority = NV_FLD_SET_DRF_NUM( DC_DISP,
                MEM_HIGH_PRIORITY, CBR_DISPLAY0A2MC_HPTH, 32,
                mem_high_priority );

            mem_high_priority_timer = NV_FLD_SET_DRF_NUM( DC_DISP,
                MEM_HIGH_PRIORITY_TIMER, CBR_DISPLAY0A2MC_HPTM, 1,
                mem_high_priority_timer );
            break;
        case DC_HAL_WIN_B:
            mem_high_priority = NV_FLD_SET_DRF_NUM( DC_DISP,
                MEM_HIGH_PRIORITY, CBR_DISPLAYB2MC_HPTH, 32,
                mem_high_priority );

            mem_high_priority_timer = NV_FLD_SET_DRF_NUM( DC_DISP,
                MEM_HIGH_PRIORITY_TIMER, CBR_DISPLAYB2MC_HPTM, 1,
                mem_high_priority_timer );
            break;
        case DC_HAL_WIN_C:
            mem_high_priority = NV_FLD_SET_DRF_NUM( DC_DISP,
                MEM_HIGH_PRIORITY, CBR_DISPLAY0C2MC_HPTH, 32,
                mem_high_priority );

            mem_high_priority_timer = NV_FLD_SET_DRF_NUM( DC_DISP,
                MEM_HIGH_PRIORITY_TIMER, CBR_DISPLAY0C2MC_HPTM, 1,
                mem_high_priority_timer );
            break;
        default:
            // cursor, etc, ignore
            bUpdateBlend = NV_FALSE;
            return;
        }

        DC_WRITE( cont, DC_DISP_MEM_HIGH_PRIORITY_0, mem_high_priority );
        DC_WRITE( cont, DC_DISP_MEM_HIGH_PRIORITY_TIMER_0,
            mem_high_priority_timer );

        cont->mem_high_priority = mem_high_priority;
        cont->mem_high_priority_timer = mem_high_priority_timer;
    }
    else
    {
        cont->win_enable &= ~(DC_WIN_MASK( window ));

        // FIXME: disable high-priority client:
        // 1) null the window via alpha blending
        // 2) set thresh to 0
        // 3) wait a frame
        // 4) disable the window hardware
    }

    if( bUpdateBlend )
    {
        cont->update_blend = NV_TRUE;
    }
}

void
DcPushSetcl( DcController *cont )
{
    NvU32 class_id;
    class_id = ( cont->index == 0 ) ? NV_DISPLAY_CLASS_ID :
        NV_DISPLAYB_CLASS_ID;

    /* set the current class to display */
    NVRM_STREAM_PUSH_U( cont->pb,
        NVRM_CH_OPCODE_SET_CLASS( class_id, 0, 0 ) );
}

/**
 * These block until the display registers are safe to write.
 */
void
DcRegWrSafe_Stream( DcController *cont )
{
    NvRmStream *stream;
    NvU32 val;

    /* a null pushbufer should never happen -- here to appease coverity */
    if( !cont->pb )
    {
        NV_ASSERT( !"no pushbuffer allocated" );
        return;
    }

    stream = (cont->stream_fw) ? cont->stream_fw : &cont->stream;

    if( NVDDK_DISP_CAP_IS_SET( cont->caps,
            NvDdkDispCapBit_Bug_Double_Syncpt_Incr ) )
    {
        cont->SyncPointShadow += 2;

        DcLatch( cont, NV_FALSE );

        val = NV_DRF_DEF( DC_CMD, GENERAL_INCR_SYNCPT, GENERAL_COND,
                REG_WR_SAFE )
            | NV_DRF_NUM( DC_CMD, GENERAL_INCR_SYNCPT, GENERAL_INDX,
                cont->SyncPointId );

        DC_WRITE_STREAM( cont, DC_CMD_GENERAL_INCR_SYNCPT_0, val, cont->pb );
        DC_WRITE_STREAM( cont, DC_CMD_GENERAL_INCR_SYNCPT_0, val, cont->pb );
    }
    else
    {
        cont->SyncPointShadow++;

        val = NV_DRF_DEF( DC_CMD, GENERAL_INCR_SYNCPT, GENERAL_COND,
                REG_WR_SAFE )
            | NV_DRF_NUM( DC_CMD, GENERAL_INCR_SYNCPT, GENERAL_INDX,
                cont->SyncPointId );

        DC_WRITE_STREAM( cont, DC_CMD_GENERAL_INCR_SYNCPT_0, val, cont->pb );
    }

    NVRM_STREAM_PUSH_U( cont->pb,
         NVRM_CH_OPCODE_SET_CLASS( NV_HOST1X_CLASS_ID, 0, 0 ) );

    val = NV_DRF_NUM( NV_CLASS_HOST, WAIT_SYNCPT, INDX,
            cont->SyncPointId )
        | NV_DRF_NUM( NV_CLASS_HOST, WAIT_SYNCPT, THRESH,
            cont->SyncPointShadow );

    NVRM_STREAM_PUSH_U( cont->pb,
        NVRM_CH_OPCODE_NONINCR( NV_CLASS_HOST_WAIT_SYNCPT_0, 1 ) );
    NVRM_STREAM_PUSH_U( cont->pb, val );

    NVRM_STREAM_PUSH_WAIT_CHECK( stream, cont->pb, cont->SyncPointId,
                cont->SyncPointShadow );

    DcPushSetcl( cont );
}

void
DcRegWrSafe_Direct( DcController *cont )
{
    NvU32 val;
    NvU32 reg;

    HOST_CLOCK_ENABLE( cont );

    reg = DC_CMD_GENERAL_INCR_SYNCPT_0 * 4;

    val = NV_DRF_DEF( DC_CMD, GENERAL_INCR_SYNCPT, GENERAL_COND,
            REG_WR_SAFE )
        | NV_DRF_NUM( DC_CMD, GENERAL_INCR_SYNCPT, GENERAL_INDX,
            cont->SyncPointId );

    if( NVDDK_DISP_CAP_IS_SET( cont->caps,
            NvDdkDispCapBit_Bug_Double_Syncpt_Incr ) )
    {
        cont->SyncPointShadow += 2;
        DcLatch( cont, NV_TRUE );
        DcRegWr( cont, 1, &reg, &val );
        DcRegWr( cont, 1, &reg, &val );
    }
    else
    {
        cont->SyncPointShadow++;
        DcRegWr( cont, 1, &reg, &val );
    }

    NvRmChannelSyncPointWait( cont->hRm, cont->SyncPointId,
        cont->SyncPointShadow, cont->sem );

    HOST_CLOCK_DISABLE( cont );
}

void
DcRegWrSafe( DcController *cont )
{
    if( NVDDK_DISP_CAP_IS_SET( cont->caps, NvDdkDispCapBit_IndirectRegs ) )
    {
        DcRegWrSafe_Stream( cont );
    }
    else
    {
        DcRegWrSafe_Direct( cont );
    }
}

/**
 * Most of the registers in the display hardware are shadowed and need to be
 * explicitly latched into the active register set.
 */
void
DcLatch( DcController *cont, NvBool bForceDirect )
{
    NvU32 val0, val1;

    val0 = NV_DRF_DEF( DC_CMD, STATE_CONTROL, GENERAL_ACT_REQ, ENABLE );
    val1 = NV_DRF_DEF( DC_CMD, STATE_CONTROL, GENERAL_UPDATE, ENABLE );

    if( cont->win_dirty & DC_WIN_MASK( DC_HAL_WIN_A ) )
    {
        val0 = NV_FLD_SET_DRF_DEF( DC_CMD, STATE_CONTROL, WIN_A_ACT_REQ,
            ENABLE, val0 );
        val1 = NV_FLD_SET_DRF_DEF( DC_CMD, STATE_CONTROL, WIN_A_UPDATE,
            ENABLE, val1 );
    }
    if( cont->win_dirty & DC_WIN_MASK( DC_HAL_WIN_B ) )
    {
        val0 = NV_FLD_SET_DRF_DEF( DC_CMD, STATE_CONTROL, WIN_B_ACT_REQ,
            ENABLE, val0 );
        val1 = NV_FLD_SET_DRF_DEF( DC_CMD, STATE_CONTROL, WIN_B_UPDATE,
            ENABLE, val1 );
    }
    if( cont->win_dirty & DC_WIN_MASK( DC_HAL_WIN_C ) )
    {
        val0 = NV_FLD_SET_DRF_DEF( DC_CMD, STATE_CONTROL, WIN_C_ACT_REQ,
            ENABLE, val0 );
        val1 = NV_FLD_SET_DRF_DEF( DC_CMD, STATE_CONTROL, WIN_C_UPDATE,
            ENABLE, val1 );
    }

    if( bForceDirect || !cont->pb )
    {
        DC_WRITE_DIRECT( cont, DC_CMD_STATE_CONTROL_0, val1 );
        DC_WRITE_DIRECT( cont, DC_CMD_STATE_CONTROL_0, val0 );
    }
    else
    {
        DC_WRITE_STREAM( cont, DC_CMD_STATE_CONTROL_0, val1, cont->pb );
        DC_WRITE_STREAM( cont, DC_CMD_STATE_CONTROL_0, val0, cont->pb );
    }

    cont->win_dirty = 0;
}

void
DcSetLatchMux( DcController *cont )
{
    NvU32 val;

    /* reads and writes go to assembly state */
    val = NV_DRF_DEF( DC_CMD, STATE_ACCESS, READ_MUX, ASSEMBLY )
        | NV_DRF_DEF( DC_CMD, STATE_ACCESS, WRITE_MUX, ASSEMBLY );
    DC_WRITE_DIRECT( cont, DC_CMD_STATE_ACCESS_0, val );

    DcLatch( cont, NV_TRUE );
    DC_FLUSH_DIRECT( cont );

    DcRegWrSafe_Direct( cont );

    /* setup register activation time */
    val = NV_DRF_DEF( DC_CMD, REG_ACT_CONTROL, GENERAL_ACT_CNTR_SEL,
            VCOUNTER )
        | NV_DRF_DEF( DC_CMD, REG_ACT_CONTROL, WIN_A_ACT_CNTR_SEL,
            VCOUNTER )
        | NV_DRF_DEF( DC_CMD, REG_ACT_CONTROL, WIN_B_ACT_CNTR_SEL,
            VCOUNTER )
        | NV_DRF_DEF( DC_CMD, REG_ACT_CONTROL, WIN_C_ACT_CNTR_SEL,
            VCOUNTER );
    DC_WRITE_DIRECT( cont, DC_CMD_REG_ACT_CONTROL_0, val );

    DcLatch( cont, NV_TRUE );
    DC_FLUSH_DIRECT( cont );
}

/* this is meant for down stream display engines (tvo and hdmi) and avoids
 * the usual means of writing to hardware.
 *
 * Begin() must be called prior to this.
 */
void
DcStopDisplay( DcController *cont, NvBool bWaitFrame )
{
    NvU32 regs[3];
    NvU32 vals[3];
    NvU32 *tmp;

    tmp = regs;
    *tmp = DC_CMD_DISPLAY_COMMAND_0 * 4; tmp++;
    *tmp = DC_CMD_STATE_CONTROL_0 * 4; tmp++;
    *tmp = DC_CMD_STATE_CONTROL_0 * 4;

    tmp = vals;
    *tmp = NV_DRF_DEF( DC_CMD, DISPLAY_COMMAND, DISPLAY_CTRL_MODE, STOP );
    tmp++;
    *tmp = NV_DRF_DEF( DC_CMD, STATE_CONTROL, GENERAL_ACT_REQ, ENABLE );
    tmp++;
    *tmp = NV_DRF_DEF( DC_CMD, STATE_CONTROL, GENERAL_UPDATE, ENABLE );

    HOST_CLOCK_ENABLE( cont );

    NvRmHostModuleRegWr( cont->hRm,
        NVRM_MODULE_ID( NvRmModuleID_Display, cont->index ),
        NV_ARRAY_SIZE(regs), regs, vals );

    if( bWaitFrame )
    {
        NvBool bFlush = NV_FALSE;
        tmp = regs;

        if( cont->pb )
        {
            bFlush = NV_TRUE;
        }
        DC_END( cont );
        if( bFlush )
        {
            NvRmStreamFlush( &cont->stream, 0 );
        }

        cont->SyncPointShadow += 2;

        *tmp = DC_CMD_GENERAL_INCR_SYNCPT_0 * 4; tmp++;
        *tmp = DC_CMD_GENERAL_INCR_SYNCPT_0 * 4; tmp++;

        if( NVDDK_DISP_CAP_IS_SET( cont->caps,
                NvDdkDispCapBit_Bug_Double_Syncpt_Incr ) )
        {
            NvU32 r;
            r = NV_DRF_DEF( DC_CMD, GENERAL_INCR_SYNCPT, GENERAL_COND,
                    REG_WR_SAFE )
                | NV_DRF_NUM( DC_CMD, GENERAL_INCR_SYNCPT, GENERAL_INDX,
                    cont->SyncPointId );

            tmp = vals;
            *tmp = r;
            tmp++;
            *tmp = r;
        }
        else
        {
            tmp = vals;
            *tmp = NV_DRF_DEF( DC_CMD, GENERAL_INCR_SYNCPT, GENERAL_COND,
                    FRAME_START )
                | NV_DRF_NUM( DC_CMD, GENERAL_INCR_SYNCPT, GENERAL_INDX,
                    cont->SyncPointId );
            tmp++;
            *tmp = NV_DRF_DEF( DC_CMD, GENERAL_INCR_SYNCPT, GENERAL_COND,
                    FRAME_DONE )
                | NV_DRF_NUM( DC_CMD, GENERAL_INCR_SYNCPT, GENERAL_INDX,
                    cont->SyncPointId );
        }

        NvRmHostModuleRegWr( cont->hRm,
            NVRM_MODULE_ID( NvRmModuleID_Display, cont->index ),
            2, regs, vals );

        NvRmChannelSyncPointWait( cont->hRm, cont->SyncPointId,
            cont->SyncPointShadow, cont->sem );

        DC_BEGIN( cont );
    }

    HOST_CLOCK_DISABLE( cont );
}

NvU32
DcGetInterruptType( DcController *cont )
{
    NvU32 reg, val, tmp;
    NvU32 e = 0;

    reg = DC_CMD_INT_STATUS_0 * 4;
    DcRegRd( cont, 1, &reg, &val );

    /* all three windows in one shot */
    tmp = NV_DRF_NUM( DC_CMD, INT_STATUS, WIN_A_UF_INT, 1 )
        | NV_DRF_NUM( DC_CMD, INT_STATUS, WIN_B_UF_INT, 1 )
        | NV_DRF_NUM( DC_CMD, INT_STATUS, WIN_C_UF_INT, 1 );
    if( tmp & val )
    {
        e |= NvDdkDispActiveError_Underflow;
    }

    tmp = NV_DRF_NUM( DC_CMD, INT_STATUS, WIN_A_OF_INT, 1 )
        | NV_DRF_NUM( DC_CMD, INT_STATUS, WIN_B_OF_INT, 1 )
        | NV_DRF_NUM( DC_CMD, INT_STATUS, WIN_C_OF_INT, 1 );
    if( tmp & val )
    {
        e |= NvDdkDispActiveError_Overflow;
    }

    return e;
}

void
DcInterruptHandler( void *args )
{
    DcController *cont;
    NvU32 reg;
    NvU32 e, val;
    NvBool bSig = NV_FALSE;
    NvBool bIntSig = NV_FALSE;

    cont = (DcController *)args;

    NV_ASSERT( cont->InterruptHandle );
    NV_DEBUG_CODE(
        if( cont->bInternalErrorEnable )
        {
            NV_ASSERT( cont->InternalSem );
        }
    );

    NvOsIntrMutexLock( cont->IntrMutex );

    NV_ASSERT_SUCCESS(
        NvRmPowerModuleClockControl( cont->hRm,
            NVRM_MODULE_ID( NvRmModuleID_Display, cont->index ),
            cont->PowerClientId,
            NV_TRUE )
    );

    e = DcGetInterruptType( cont );

    if( e & NvDdkDispActiveError_Underflow )
    {
        if( cont->bInternalErrorEnable )
        {
            cont->InternalError |= NvDdkDispActiveError_Underflow;
            bIntSig = NV_TRUE;
        }

        if( (cont->bErrorTriggerEnable) &&
            (cont->InterruptBits & NvDdkDispActiveError_Underflow) )
        {
            cont->InterruptActiveErrors |= NvDdkDispActiveError_Underflow;
            bSig = NV_TRUE;
        }
    }

    if( e & NvDdkDispActiveError_Overflow )
    {
        if( cont->bInternalErrorEnable )
        {
            cont->InternalError |= NvDdkDispActiveError_Overflow;
            bIntSig = NV_TRUE;
        }

        if( (cont->bErrorTriggerEnable) &&
            (cont->InterruptBits & NvDdkDispActiveError_Overflow) )
        {
            cont->InterruptActiveErrors |= NvDdkDispActiveError_Overflow;
            bSig = NV_TRUE;
        }
    }

    /* mask interrupts */
    val = 0;
    reg = DC_CMD_INT_MASK_0 * 4;
    DcRegWr( cont, 1, &reg, &val );

    /* clear interrupts */
    reg = DC_CMD_INT_STATUS_0 * 4;
    val = 0xFFFFFFFF;
    DcRegWr( cont, 1, &reg, &val );

    NV_ASSERT_SUCCESS(
        NvRmPowerModuleClockControl( cont->hRm,
            NVRM_MODULE_ID( NvRmModuleID_Display, cont->index ),
            cont->PowerClientId,
            NV_FALSE )
    );

    /* only allow the error handler to stay in lock-step with
     * the interrupt count, otherwise the semaphore value will
     * be very large.
     */
    cont->bErrorTriggerEnable = NV_FALSE;
    cont->bInternalErrorEnable = NV_FALSE;

    NvOsIntrMutexUnlock( cont->IntrMutex );
    NvRmInterruptDone( cont->InterruptHandle );

    if( bSig )
    {
        NvOsSemaphoreSignal( cont->InterruptSem );
    }

    if( bIntSig )
    {
        NvOsSemaphoreSignal( cont->InternalSem );
    }
}

void
DcSetInterrupts( DcController *cont )
{
    NvU32 val;
    NvU32 IrqList;
    NvOsInterruptHandler handler;

    if( !cont->InterruptHandle )
    {
        /* install an interrupt handler */
        handler = DcInterruptHandler;
        IrqList = NvRmGetIrqForLogicalInterrupt( cont->hRm,
                 NVRM_MODULE_ID( NvRmModuleID_Display, cont->index ), 0 );

        // FIXME: need to handle this potential error
        NV_ASSERT_SUCCESS(
            NvRmInterruptRegister( cont->hRm, 1, &IrqList, &handler, cont,
                &cont->InterruptHandle, NV_TRUE )
        );

        /* clear all pending just in case */
        val = 0xFFFFFFFF;
        DC_WRITE( cont, DC_CMD_INT_STATUS_0, val );
    }

    // some environments don't support interrupts (eboot)
    if( cont->InterruptHandle == 0 )
    {
        return;
    }

    /* underflow and overflow */
    val = NV_DRF_DEF( DC_CMD, INT_MASK, WIN_A_UF_INT_MASK, NOTMASKED )
        | NV_DRF_DEF( DC_CMD, INT_MASK, WIN_B_UF_INT_MASK, NOTMASKED )
        | NV_DRF_DEF( DC_CMD, INT_MASK, WIN_C_UF_INT_MASK, NOTMASKED )
        | NV_DRF_DEF( DC_CMD, INT_MASK, WIN_A_OF_INT_MASK, NOTMASKED )
        | NV_DRF_DEF( DC_CMD, INT_MASK, WIN_B_OF_INT_MASK, NOTMASKED )
        | NV_DRF_DEF( DC_CMD, INT_MASK, WIN_C_OF_INT_MASK, NOTMASKED );
    DC_WRITE( cont, DC_CMD_INT_MASK_0, val );

    val = NV_DRF_DEF( DC_CMD, INT_TYPE, WIN_A_UF_INT_TYPE, LEVEL )
        | NV_DRF_DEF( DC_CMD, INT_TYPE, WIN_B_UF_INT_TYPE, LEVEL )
        | NV_DRF_DEF( DC_CMD, INT_TYPE, WIN_C_UF_INT_TYPE, LEVEL )
        | NV_DRF_DEF( DC_CMD, INT_TYPE, WIN_A_OF_INT_TYPE, LEVEL )
        | NV_DRF_DEF( DC_CMD, INT_TYPE, WIN_B_UF_INT_TYPE, LEVEL )
        | NV_DRF_DEF( DC_CMD, INT_TYPE, WIN_C_UF_INT_TYPE, LEVEL );
    DC_WRITE( cont, DC_CMD_INT_TYPE_0, val );

    val = NV_DRF_DEF( DC_CMD, INT_POLARITY, WIN_A_UF_INT_POLARITY, HIGH )
        | NV_DRF_DEF( DC_CMD, INT_POLARITY, WIN_B_UF_INT_POLARITY, HIGH )
        | NV_DRF_DEF( DC_CMD, INT_POLARITY, WIN_C_UF_INT_POLARITY, HIGH )
        | NV_DRF_DEF( DC_CMD, INT_POLARITY, WIN_A_OF_INT_POLARITY, HIGH )
        | NV_DRF_DEF( DC_CMD, INT_POLARITY, WIN_B_OF_INT_POLARITY, HIGH )
        | NV_DRF_DEF( DC_CMD, INT_POLARITY, WIN_C_OF_INT_POLARITY, HIGH );
    DC_WRITE( cont, DC_CMD_INT_POLARITY_0, val );

    /* enable should be last */
    val = NV_DRF_DEF( DC_CMD, INT_ENABLE, WIN_A_UF_INT_ENABLE, ENABLE )
        | NV_DRF_DEF( DC_CMD, INT_ENABLE, WIN_B_UF_INT_ENABLE, ENABLE )
        | NV_DRF_DEF( DC_CMD, INT_ENABLE, WIN_C_UF_INT_ENABLE, ENABLE )
        | NV_DRF_DEF( DC_CMD, INT_ENABLE, WIN_A_OF_INT_ENABLE, ENABLE )
        | NV_DRF_DEF( DC_CMD, INT_ENABLE, WIN_B_OF_INT_ENABLE, ENABLE )
        | NV_DRF_DEF( DC_CMD, INT_ENABLE, WIN_C_OF_INT_ENABLE, ENABLE );
    DC_WRITE( cont, DC_CMD_INT_ENABLE_0, val );
}

void
DcSetMcConfig( DcController *cont )
{
    NvU32 val;

    /* WARNING: DO NOT *EVER* WRITE ANYTHING NON-ZERO INTO THIS REGISTER! */
    val = 0;
    DC_WRITE( cont, DC_DISP_DC_MCCIF_FIFOCTRL_0, val );

    cont->mem_high_priority = 0;
    cont->mem_high_priority_timer = 0;

    DC_WRITE( cont, DC_DISP_MEM_HIGH_PRIORITY_0, 0 );
    DC_WRITE( cont, DC_DISP_MEM_HIGH_PRIORITY_TIMER_0, 0 );
}

void
DcSetPanelPower( DcController *cont, NvBool enable )
{
    NvU32 val;

    if( enable )
    {
        val = NV_DRF_DEF( DC_CMD, DISPLAY_POWER_CONTROL, PW0_ENABLE, ENABLE ) |
            NV_DRF_DEF( DC_CMD, DISPLAY_POWER_CONTROL, PW1_ENABLE, ENABLE ) |
            NV_DRF_DEF( DC_CMD, DISPLAY_POWER_CONTROL, PW2_ENABLE, ENABLE ) |
            NV_DRF_DEF( DC_CMD, DISPLAY_POWER_CONTROL, PW3_ENABLE, ENABLE ) |
            NV_DRF_DEF( DC_CMD, DISPLAY_POWER_CONTROL, PW4_ENABLE, ENABLE ) |
            NV_DRF_DEF( DC_CMD, DISPLAY_POWER_CONTROL, PM0_ENABLE, ENABLE ) |
            NV_DRF_DEF( DC_CMD, DISPLAY_POWER_CONTROL, PM1_ENABLE, ENABLE );
    }
    else
    {
        val =
            NV_DRF_DEF( DC_CMD, DISPLAY_POWER_CONTROL, PW0_ENABLE, DISABLE ) |
            NV_DRF_DEF( DC_CMD, DISPLAY_POWER_CONTROL, PW1_ENABLE, DISABLE ) |
            NV_DRF_DEF( DC_CMD, DISPLAY_POWER_CONTROL, PW2_ENABLE, DISABLE ) |
            NV_DRF_DEF( DC_CMD, DISPLAY_POWER_CONTROL, PW3_ENABLE, DISABLE ) |
            NV_DRF_DEF( DC_CMD, DISPLAY_POWER_CONTROL, PW4_ENABLE, DISABLE ) |
            NV_DRF_DEF( DC_CMD, DISPLAY_POWER_CONTROL, PM0_ENABLE, DISABLE ) |
            NV_DRF_DEF( DC_CMD, DISPLAY_POWER_CONTROL, PM1_ENABLE, DISABLE );
    }

    DC_WRITE( cont, DC_CMD_DISPLAY_POWER_CONTROL_0, val );
}

/**
 * Each hardware window has a copy of the window registers.  Need to select
 * which window to write to via the window_header register.
 */
void
DcSetWindow( DcController *cont, NvU32 window )
{
    NvU32 val = 0;
    NV_ASSERT( cont );

    if( window == DC_HAL_WIN_A )
    {
        val = NV_DRF_DEF( DC_CMD, DISPLAY_WINDOW_HEADER, WINDOW_A_SELECT,
            ENABLE );
    }
    else if( window == DC_HAL_WIN_B )
    {
        val = NV_DRF_DEF( DC_CMD, DISPLAY_WINDOW_HEADER, WINDOW_B_SELECT,
            ENABLE );
    }
    else if( window == DC_HAL_WIN_C )
    {
        val = NV_DRF_DEF( DC_CMD, DISPLAY_WINDOW_HEADER, WINDOW_C_SELECT,
            ENABLE );
    }

    if( cont->pb )
    {
        DC_WRITE_STREAM( cont, DC_CMD_DISPLAY_WINDOW_HEADER_0, val, cont->pb );
    }
    else
    {
        DC_WRITE( cont, DC_CMD_DISPLAY_WINDOW_HEADER_0, val );
    }
}

/**
 * Note on color formats: nvcolor.h specifies colors in msb-first whilst the
 * hardware specifies colors lsb-first.  Therefore, NvColorFormat_A4R4G4B4
 * is the same as DC_WIN_B_COLOR_DEPTH_0_B_COLOR_DEPTH_B6G4R4A4.
 *
 * This does not actually flush the registers to hardware.
 */
NvBool
DcSetColorDepth( DcController *cont, DcWindow *win,
    NvRmSurface *surf, NvU32 count )
{
    NvU32 val = 0;
    NvU32 enable_csc = NV_FALSE;
    NvU32 enable_color_expand = NV_TRUE;
    NvBool enable_byte_swap2 = NV_FALSE;
    NvBool enable_byte_swap4 = NV_FALSE;
    NvBool enable_byte_swap4hw = NV_FALSE;
    NvBool alpha_format = NV_FALSE;
    NvBool uv_align = NV_FALSE;

    NV_ASSERT( cont );
    NV_ASSERT( win );
    NV_ASSERT( surf );

    // FIXME: endian NvColorFormat_R8_G8_B8 and NvColorFormat_B8_G8_R8?

    switch( surf->ColorFormat ) {
    case NvColorFormat_A4R4G4B4:
        alpha_format = NV_TRUE;
        val = NV_DRF_DEF( DC_WIN, B_COLOR_DEPTH, B_COLOR_DEPTH, B4G4R4A4 );
        break;
    case NvColorFormat_A1R5G5B5:
        alpha_format = NV_TRUE;
        val = NV_DRF_DEF( DC_WIN, B_COLOR_DEPTH, B_COLOR_DEPTH, B5G5R5A );
        break;
    case NvColorFormat_R5G5B5A1:
        alpha_format = NV_TRUE;
        // FIXME: should support this with byte swap?
        NV_ASSERT( !"fixme" );
        break;
    case NvColorFormat_R5G6B5:
        val = NV_DRF_DEF( DC_WIN, B_COLOR_DEPTH, B_COLOR_DEPTH, B5G6R5 );
        break;
    case NvColorFormat_A8B8G8R8:
        alpha_format = NV_TRUE;
        enable_color_expand = NV_FALSE;
        val = NV_DRF_DEF( DC_WIN, B_COLOR_DEPTH, B_COLOR_DEPTH, R8G8B8A8 );
        break;
    case NvColorFormat_A8R8G8B8:
        alpha_format = NV_TRUE;
        enable_color_expand = NV_FALSE;
        val = NV_DRF_DEF( DC_WIN, B_COLOR_DEPTH, B_COLOR_DEPTH, B8G8R8A8 );
        break;
    case NvColorFormat_R8G8B8A8:
        alpha_format = NV_TRUE;
        enable_color_expand = NV_FALSE;
        enable_byte_swap4 = NV_TRUE;
        val = NV_DRF_DEF( DC_WIN, B_COLOR_DEPTH, B_COLOR_DEPTH, R8G8B8A8 );
        break;
    case NvColorFormat_B8G8R8A8:
        alpha_format = NV_TRUE;
        enable_color_expand = NV_FALSE;
        enable_byte_swap4 = NV_TRUE;
        val = NV_DRF_DEF( DC_WIN, B_COLOR_DEPTH, B_COLOR_DEPTH, B8G8R8A8 );
        break;
    case NvColorFormat_X8R8G8B8:
        alpha_format = NV_FALSE;
        enable_color_expand = NV_FALSE;
        val = NV_DRF_DEF( DC_WIN, B_COLOR_DEPTH, B_COLOR_DEPTH, B8G8R8A8 );
        break;
    case NvColorFormat_VYUY:
        enable_csc = NV_TRUE;
        enable_color_expand = NV_FALSE;
        enable_byte_swap4hw = NV_TRUE;
        val = NV_DRF_DEF( DC_WIN, B_COLOR_DEPTH, B_COLOR_DEPTH, YCbCr422 );
        break;
    case NvColorFormat_YVYU:
        enable_csc = NV_TRUE;
        enable_color_expand = NV_FALSE;
        enable_byte_swap4 = NV_TRUE;
        val = NV_DRF_DEF( DC_WIN, B_COLOR_DEPTH, B_COLOR_DEPTH, YCbCr422 );
        break;
    case NvColorFormat_UYVY:
        enable_csc = NV_TRUE;
        enable_color_expand = NV_FALSE;
        val = NV_DRF_DEF( DC_WIN, B_COLOR_DEPTH, B_COLOR_DEPTH, YCbCr422 );
        break;
    case NvColorFormat_YUYV:
        enable_csc = NV_TRUE;
        enable_color_expand = NV_FALSE;
        enable_byte_swap2 = NV_TRUE;
        val = NV_DRF_DEF( DC_WIN, B_COLOR_DEPTH, B_COLOR_DEPTH, YCbCr422 );
        break;
    case NvColorFormat_Y8:
    {
        NvYuvColorFormat format;
        NvRmSurface *surfs[3];

        NV_ASSERT( count == 3 );
        NV_ASSERT( surf[1].ColorFormat == NvColorFormat_U8 );
        NV_ASSERT( surf[2].ColorFormat == NvColorFormat_V8 );

        enable_csc = NV_TRUE;
        enable_color_expand = NV_FALSE;

        /* yuv:420 chroma width and height is half of luma
         * yuv:422 chroma width is half of luma, height is the same
         * yuv:422R chroma height is half of luma, width is the same
         */
        surfs[0] = &surf[0];
        surfs[1] = &surf[1];
        surfs[2] = &surf[2];
        format = NvRmSurfaceGetYuvColorFormat( surfs, count );

        if( format == NvYuvColorFormat_YUV420 )
        {
            val = NV_DRF_DEF( DC_WIN, B_COLOR_DEPTH, B_COLOR_DEPTH,
                YCbCr420P );
        }
        else if( format == NvYuvColorFormat_YUV422 )
        {
            val = NV_DRF_DEF( DC_WIN, B_COLOR_DEPTH, B_COLOR_DEPTH,
                YCbCr422P );
        }
        else if( format == NvYuvColorFormat_YUV422R )
        {
            val = NV_DRF_DEF( DC_WIN, B_COLOR_DEPTH, B_COLOR_DEPTH,
                YCbCr422R );
        }
        else
        {
            NV_ASSERT( !"unsupported yuv format" );
        }

        uv_align = NV_TRUE;
        break;
    }
    default:
        NV_ASSERT( !"unsupported yuv format" );
        return NV_FALSE;
    }

    DC_WRITE( cont, DC_WIN_B_COLOR_DEPTH_0, val );

    win->bAlphaFormat = alpha_format;

    val = win->win_options;
    if( enable_csc )
    {
        val = NV_FLD_SET_DRF_DEF( DC_WIN, B_WIN_OPTIONS, B_CSC_ENABLE,
            ENABLE, val );
    }
    else
    {
        val = NV_FLD_SET_DRF_DEF( DC_WIN, B_WIN_OPTIONS, B_CSC_ENABLE,
            DISABLE, val );
    }

    if( enable_color_expand )
    {
        val = NV_FLD_SET_DRF_DEF( DC_WIN, B_WIN_OPTIONS, B_COLOR_EXPAND,
            ENABLE, val );
    }
    else
    {
        val = NV_FLD_SET_DRF_DEF( DC_WIN, B_WIN_OPTIONS, B_COLOR_EXPAND,
            DISABLE, val );
    }

    if( uv_align )
    {
        val = NV_FLD_SET_DRF_DEF( DC_WIN, B_WIN_OPTIONS, B_V_FILTER_UV_ALIGN,
            ENABLE, val );
    }

    win->win_options = val;
    DC_WRITE( cont, DC_WIN_B_WIN_OPTIONS_0, val );

    if( enable_byte_swap2 )
    {
        val = NV_DRF_DEF( DC_WIN, B_BYTE_SWAP, B_BYTE_SWAP, SWAP2 );
    }
    else if( enable_byte_swap4 )
    {
        val = NV_DRF_DEF( DC_WIN, B_BYTE_SWAP, B_BYTE_SWAP, SWAP4 );
    }
    else if( enable_byte_swap4hw )
    {
        val = NV_DRF_DEF( DC_WIN, B_BYTE_SWAP, B_BYTE_SWAP, SWAP4HW );
    }
    else
    {
        val = NV_DRF_DEF( DC_WIN, B_BYTE_SWAP, B_BYTE_SWAP, NOSWAP );
    }

    DC_WRITE( cont, DC_WIN_B_BYTE_SWAP_0, val );

    if( win->bAlphaFormat )
    {
        cont->update_blend = NV_TRUE;
    }

    return NV_TRUE;
}

void
DcSetColorBase( DcController *cont, NvU32 base )
{
    NvU32 val;
    NvBool enable_dither = NV_TRUE;

    NV_ASSERT( cont );

    cont->base = base;
    val = cont->color_control;

    switch( base ) {
    case NvOdmDispBaseColorSize_111:
        val = NV_FLD_SET_DRF_DEF( DC_DISP, DISP_COLOR_CONTROL, BASE_COLOR_SIZE,
            BASE111, val );
        break;
    case NvOdmDispBaseColorSize_222:
        val = NV_FLD_SET_DRF_DEF( DC_DISP, DISP_COLOR_CONTROL, BASE_COLOR_SIZE,
            BASE222, val );
        break;
    case NvOdmDispBaseColorSize_333:
        val = NV_FLD_SET_DRF_DEF( DC_DISP, DISP_COLOR_CONTROL, BASE_COLOR_SIZE,
            BASE333, val );
        break;
    case NvOdmDispBaseColorSize_444:
        val = NV_FLD_SET_DRF_DEF( DC_DISP, DISP_COLOR_CONTROL, BASE_COLOR_SIZE,
            BASE444, val );
        break;
    case NvOdmDispBaseColorSize_555:
        val = NV_FLD_SET_DRF_DEF( DC_DISP, DISP_COLOR_CONTROL, BASE_COLOR_SIZE,
            BASE555, val );
        break;
    case NvOdmDispBaseColorSize_565:
        val = NV_FLD_SET_DRF_DEF( DC_DISP, DISP_COLOR_CONTROL, BASE_COLOR_SIZE,
            BASE565, val );
        break;
    case NvOdmDispBaseColorSize_666:
        val = NV_FLD_SET_DRF_DEF( DC_DISP, DISP_COLOR_CONTROL, BASE_COLOR_SIZE,
            BASE666, val );
        break;
    case NvOdmDispBaseColorSize_332:
        val = NV_FLD_SET_DRF_DEF( DC_DISP, DISP_COLOR_CONTROL, BASE_COLOR_SIZE,
            BASE332, val );
        break;
    case NvOdmDispBaseColorSize_888:
        enable_dither = NV_FALSE;
        val = NV_FLD_SET_DRF_DEF( DC_DISP, DISP_COLOR_CONTROL, BASE_COLOR_SIZE,
            BASE888, val );
        break;
    default:
        NV_ASSERT( !"bad base value" );
        return;
    }

    if( enable_dither )
    {
        if( NVDDK_DISP_CAP_IS_SET( cont->caps,
                NvDdkDispCapBit_LineBuffer_1280 ) &&
            cont->mode.timing.Horiz_DispActive <= 1280 )
        {
            val = NV_FLD_SET_DRF_DEF( DC_DISP, DISP_COLOR_CONTROL,
                DITHER_CONTROL, ERRDIFF, val );
        }
        else
        {
            val = NV_FLD_SET_DRF_DEF( DC_DISP, DISP_COLOR_CONTROL,
                DITHER_CONTROL, ORDERED, val );
        }
    }

    cont->color_control = val;
    DC_WRITE( cont, DC_DISP_DISP_COLOR_CONTROL_0, val );
}

static NvU8 clamp( NvU32 weight )
{
    if( weight > 255 )
    {
        return 255;
    }
    return (NvU8)weight;
}

#define DC_DEBUG_BLEND 0

static void
DcWriteBlendReg( DcController *cont, NvU32 window, NvU32 reg, NvU32 weight,
    const char *reg_name )
{
    NvU32 val = 0;
    DcWindow *win;
    NvU32 weight_0;
    NvU32 weight_1;
    const char *debug[2];
    NvU32 idx = 0;
    NvBool bWAR = NV_FALSE;

    DcSetWindow( cont, window );
    win = &cont->windows[window];

    weight_0 = weight;
    weight_1 = weight;

    // FIXME: colorkey doesn't play with alpha bending -- by disabling
    // dependent weight if there's a color key, we avoid blending the
    // colorkey color into another overlay.
    if( reg == DC_WIN_B_BLEND_3WIN_AC_0 &&
        win->blend_op == NvDdkDispBlendType_ColorKey )
    {
        bWAR = NV_TRUE;
    }
    // FIXME: this is a truely monsterous hack. use dependent weight for
    // 2win overlap for colorkey and the alpha surface, but not the colorkey
    // and the keyed overlay.
    if( !bWAR && win->bBlendWAR )
    {
        NvU32 warWindow = win->warWindow;

        // the war window is the alpha surface, setting bWAR will force fixed
        // weight, so pick the 3rd window
        switch( window ) {
        case 0:
            if( warWindow == 1 && reg == DC_WIN_B_BLEND_2WIN_C_0 )
            {
                bWAR = NV_TRUE;
            }
            if( warWindow == 2 && reg == DC_WIN_B_BLEND_2WIN_A_0 )
            {
                bWAR = NV_TRUE;
            }
            break;
        case 1:
            if( warWindow == 0 && reg == DC_WIN_B_BLEND_2WIN_C_0 )
            {
                bWAR = NV_TRUE;
            }
            if( warWindow == 2 && reg == DC_WIN_B_BLEND_2WIN_A_0 )
            {
                bWAR = NV_TRUE;
            }
            break;
        case 2:
            if( warWindow == 0 && reg == DC_WIN_B_BLEND_2WIN_C_0 )
            {
                bWAR = NV_TRUE;
            }
            if( warWindow == 1 && reg == DC_WIN_B_BLEND_2WIN_A_0 )
            {
                bWAR = NV_TRUE;
            }
            break;
        default:
            NV_ASSERT( !"bad window" );
            break;
        }
    }

    /* use blend_1win for drf (mostly) */

    if( win->bAlphaFormat &&
        win->alpha_op == NvDdkDispAlphaOperation_PremultipliedWeight &&
        reg != DC_WIN_B_BLEND_1WIN_0 )
    {
        val = NV_FLD_SET_DRF_DEF( DC_WIN, B_BLEND_2WIN_A,
                B_BLEND_CONTROL_2WIN_A, PREMULT_WEIGHT, val );
        debug[idx++] = "premult alpha";
    }
    else if( win->bAlphaFormat &&
        win->blend_op == NvDdkDispBlendType_PerPixelAlpha &&
        reg != DC_WIN_B_BLEND_1WIN_0 )
    {
        val = NV_FLD_SET_DRF_DEF( DC_WIN, B_BLEND_2WIN_A,
                B_BLEND_CONTROL_2WIN_A, ALPHA_WEIGHT, val );
        weight_0 = 0;
        weight_1 = 255;
        debug[idx++] = "alpha";
    }
    else if( win->bDependentWeight &&
        reg != DC_WIN_B_BLEND_1WIN_0 &&
        !bWAR )
    {
        val = NV_FLD_SET_DRF_DEF( DC_WIN, B_BLEND_2WIN_A,
                B_BLEND_CONTROL_2WIN_A, DEPENDENT_WEIGHT, val );
        debug[idx++] = "dependent";
    }
    else
    {
        val = NV_FLD_SET_DRF_DEF( DC_WIN, B_BLEND_1WIN, B_BLEND_CONTROL_1WIN,
                FIX_WEIGHT, val );
        debug[idx++] = "fixed";
    }

    if( reg == DC_WIN_B_BLEND_3WIN_AC_0 && win->bBlendWAR )
    {
        // See Bug 577299 on 3WIN overlap WAR.
        // Disable colorkey and change weight to 0

        val = NV_FLD_SET_DRF_DEF( DC_WIN, B_BLEND_1WIN,
            B_CKEY_ENABLE_1WIN, NOKEY, val );
        val = NV_FLD_SET_DRF_NUM( DC_WIN, B_BLEND_1WIN,
            B_BLEND_WEIGHT0_1WIN, 0x00, val );
        val = NV_FLD_SET_DRF_NUM( DC_WIN, B_BLEND_1WIN,
            B_BLEND_WEIGHT1_1WIN, 0x00, val );
        debug[idx++] = "WAR color key off";
    }
    else if( win->blend_op == NvDdkDispBlendType_ColorKey &&
        (win->bDependentWeight == NV_FALSE || bWAR) )
    {
        /* if a window is flagged as dependent weight, that means that there's
         * an alpha surface over the color key surface. in this case, the alpha
         * surface wins (should be visible).
         *
         * if bWAR is true, that means that it's actually safe to enable
         * colorkey (turns out that way, magically)
         */

        if( win->colorkey_number == 0 )
        {
            val = NV_FLD_SET_DRF_DEF( DC_WIN, B_BLEND_1WIN,
                B_CKEY_ENABLE_1WIN, CKEY0, val );
        }
        else
        {
            val = NV_FLD_SET_DRF_DEF( DC_WIN, B_BLEND_1WIN,
                B_CKEY_ENABLE_1WIN, CKEY1, val );
        }
        val = NV_FLD_SET_DRF_NUM( DC_WIN, B_BLEND_1WIN,
            B_BLEND_WEIGHT0_1WIN, 0, val );
        val = NV_FLD_SET_DRF_NUM( DC_WIN, B_BLEND_1WIN,
            B_BLEND_WEIGHT1_1WIN, 0, val );
        debug[idx++] = "color key";
    }
    else
    {
        val = NV_FLD_SET_DRF_DEF( DC_WIN, B_BLEND_1WIN,
            B_CKEY_ENABLE_1WIN, NOKEY, val );
        val = NV_FLD_SET_DRF_NUM( DC_WIN, B_BLEND_1WIN,
            B_BLEND_WEIGHT0_1WIN, clamp((weight_0)), val );
        val = NV_FLD_SET_DRF_NUM( DC_WIN, B_BLEND_1WIN,
            B_BLEND_WEIGHT1_1WIN, clamp((weight_1)), val );
        debug[idx++] = "";
    }

    if( DC_DEBUG_BLEND )
    {
        char win = 'a' + window;

        NvOsDebugPrintf( "blend window %c: %s %s %s\n", win, reg_name,
            debug[0], debug[1] );
    }

    DC_WRITE( cont, reg, val );
}

#define WRITE_BLEND(cont, window, type, weight) \
    do { \
        DcWriteBlendReg( (cont), (window), DC_WIN_B_BLEND_##type##_0, \
            (weight), #type ); \
    } while( 0 )

#define WRITE_NOKEY_BLEND(cont, window, weight) \
    do { \
        NvU32 val; \
        DcSetWindow( (cont), (window) ); \
        val = NV_DRF_NUM( DC_WIN, B_BLEND_NOKEY, B_BLEND_WEIGHT0_NOKEY, \
                clamp((weight)) ) \
            | NV_DRF_NUM( DC_WIN, B_BLEND_NOKEY, B_BLEND_WEIGHT1_NOKEY, \
                clamp((weight)) ); \
        val = NV_FLD_SET_DRF_DEF( DC_WIN, B_BLEND_NOKEY, \
            B_BLEND_CONTROL_NOKEY, FIX_WEIGHT, val ); \
        DC_WRITE( (cont), DC_WIN_B_BLEND_NOKEY_0, val ); \
    } while( 0 )

/* apply the blend/alpha operation. */
static void
DcBlendOperation( NvDdkDispBlendType blend, NvDdkDispAlphaOperation op,
    NvU32 alpha, NvU32 *weight0, NvU32 *weight1, NvU32 *weight2 )
{
    NV_ASSERT( weight0 );

    if( blend == NvDdkDispBlendType_None )
    {
        *weight0 = 256;
        if( weight1 ) *weight1 = 0;
        if( weight2 ) *weight2 = 0;
    }
    else
    {
        switch( op ) {
        case NvDdkDispAlphaOperation_WeightedMean:
            *weight0 = (*weight0 * alpha) >> 8;
            if( weight1 ) *weight1 = (*weight1 * (256 - alpha)) >> 8;
            if( weight2 ) *weight2 = (*weight2 * (256 - alpha)) >> 8;
            break;
        case NvDdkDispAlphaOperation_SimpleSum:
            *weight0 = (*weight0 * alpha) >> 8;
            break;
        case NvDdkDispAlphaOperation_PremultipliedWeight:
            *weight0 = 256;
            if( weight1 ) *weight1 = (*weight1 * (256 - alpha)) >> 8;
            if( weight2 ) *weight2 = (*weight2 * (256 - alpha)) >> 8;
            break;
        default:
            NV_ASSERT( !"bad alpha operation" );
            break;
        }
    }
}

static NvBool
rectangle_intersect( DcWindow *w0, DcWindow *w1 )
{
    NvRect *r0, *r1;
    NvPoint *p0, *p1;

    r0 = &w0->dst_rect;
    p0 = &w0->loc;
    r1 = &w1->dst_rect;
    p1 = &w1->loc;

    /* totally to the left? */
    if( (r0->right + p0->x) < (r1->left + p1->x) ) return NV_FALSE;
    /* right? */
    if( (r0->left + p0->x) > (r1->right + p1->x) ) return NV_FALSE;
    /* over? */
    if( (r0->bottom + p0->y) < (r1->top + p1->y) ) return NV_FALSE;
    /* under? */
    if( (r0->top + p0->y) > (r1->bottom + p1->y) ) return NV_FALSE;

    /* actually intersect */
    return NV_TRUE;
}

void
DcBlendWindows( DcController *cont, NvBool bReset )
{
    DcWindow *win;
    NvU32 i, j;
    NvU32 order[DC_HAL_WINDOWS];
    NvU32 weight[1<<(DC_HAL_WINDOWS+1)][DC_HAL_WINDOWS];
    NvU32 mask, bottom;
    NvBool found;

    #define NOKEY (DC_HAL_WINDOWS)

    DC_SAFE( cont, return );
    DC_DIRTY( cont, 0 );
    DC_DIRTY( cont, 1 );
    DC_DIRTY( cont, 2 );

    /* zero out the blend registers if a reset is requested */
    if( bReset )
    {
        for( i = 0; i < DC_HAL_WINDOWS; i++ )
        {
            WRITE_NOKEY_BLEND( cont, i, 0 );
            WRITE_BLEND( cont, i, 1WIN, 0 );
            WRITE_BLEND( cont, i, 3WIN_AC, 0 );
        }

        WRITE_BLEND( cont, 0, 2WIN_A, 0 );
        WRITE_BLEND( cont, 1, 2WIN_A, 0 );
        WRITE_BLEND( cont, 2, 2WIN_C, 0 );
        WRITE_BLEND( cont, 1, 2WIN_C, 0 );
        WRITE_BLEND( cont, 0, 2WIN_C, 0 );
        WRITE_BLEND( cont, 2, 2WIN_A, 0 );
        return;
    }

    if( DC_DEBUG_BLEND )
    {
        NvOsDebugPrintf( "--- blend ---\n" );
    }

    /*
     * The windows are sorted by their depth attribute. Depth 0 is on top of
     * every other window.
     */

    /* default sort order */
    for( i = 0; i < DC_HAL_WINDOWS; i++ )
    {
        order[i] = i;
    }

    /* bubble-sort the windows */
    for( i = 0; i < DC_HAL_WINDOWS; i++ )
    {
        for( j = 0; j < DC_HAL_WINDOWS - 1; j++ )
        {
            DcWindow *w0;
            DcWindow *w1;

            w0 = &cont->windows[ order[j] ];
            w1 = &cont->windows[ order[j+1] ];

            /* lowest depth is on top, window index is tie breaker for
             * equvilent depths
             */
            if( w0->depth > w1->depth ||
                ( w0->depth == w1->depth &&
                  w0->index > w1->index ) )
            {
                NvU32 tmp = order[j];
                order[j] = order[j+1];
                order[j+1] = tmp;
            }
        }
    }

    /* check for color key blends, write keys to hardware,
     * set dependent weight to default
     */
    for( i = 0, j = 0; i < DC_HAL_WINDOWS; i++ )
    {
        if( cont->windows[i].blend_op == NvDdkDispBlendType_ColorKey &&
            cont->win_enable & DC_WIN_MASK(i) )
        {
            NV_ASSERT( j < DC_HAL_MAX_COLOR_KEYS );
            cont->windows[i].colorkey_number = (NvU8)j;
            j++;

            DcSetColorKey( cont, &cont->windows[i] );
        }
        cont->windows[i].bDependentWeight = NV_FALSE;
    }

    /* set the window's dependent weight flag in PerPixelAlpha cases. */
    for( i = DC_HAL_WINDOWS - 1; (NvS32)i >= 0; i-- )
    {
        DcWindow *w, *w1;
        w = &cont->windows[ order[i] ];

        if( (cont->win_enable & DC_WIN_MASK(order[i])) == 0 ||
            w->blend_op != NvDdkDispBlendType_PerPixelAlpha )
        {
            continue;
        }

        /* find an intersecting rectangle */

        if( w->alpha_dir == NvDdkDispAlphaBlendDirection_Source )
        {
            // source alpha
            for( j = i; j < DC_HAL_WINDOWS - 1; j++ )
            {
                w1 = &cont->windows[ order[j+1] ];
                if( rectangle_intersect( w, w1 ) )
                {
                    w1->bDependentWeight = NV_TRUE;
                }
                else
                {
                    /* clear potentially old state */
                    w1->bDependentWeight = NV_FALSE;
                }
            }
        }
        else
        {
            // dest alpha
            for( j = i; j > 0; j-- )
            {
                w1 = &cont->windows[ order[j-1] ];
                if( rectangle_intersect( w, w1 ) )
                {
                    w1->bDependentWeight = NV_TRUE;
                }
                else
                {
                    w1->bDependentWeight = NV_FALSE;
                }
            }
        }
    }

    /* this looks for the case where there's potentially a colorkey, an
     * overlay, and an alpha surface. the 2win case for colorkey and alpha
     * need to be 'dependent', but the 2win case for colorkey and overlay need
     * to be 'fixed' otherwise the colorkey will either bleed through or the
     * alpha surface will not get blended with the colorkey surface.
     */
    found = NV_FALSE;
    for( i = DC_HAL_WINDOWS - 1; (NvS32)i >= 0; i-- )
    {
        DcWindow *w, *w1;
        w = &cont->windows[ order[i] ];

        w->bBlendWAR = NV_FALSE;
        w->warWindow = (NvU32)-1;

        if( found )
        {
            // bBlendWAR win already found. Skip
            continue;
        }

        // find the colorkey surface (w)
        if( (cont->win_enable & DC_WIN_MASK(order[i])) == 0 ||
            w->blend_op != NvDdkDispBlendType_ColorKey )
        {
            continue;
        }

        // find the alpha surface (w1)
        for( j = 0; j < DC_HAL_WINDOWS - 1; j++ )
        {
            if( j == order[i] )
            {
                continue;
            }

            w1 = &cont->windows[j];
            if( (cont->win_enable & DC_WIN_MASK(j)) == 0 ||
                w1->blend_op != NvDdkDispBlendType_PerPixelAlpha )
            {
                continue;
            }

            if( rectangle_intersect( w, w1 ) )
            {
                if (w1->depth < w->depth) {
                    // Apply this WAR only if alpha window (w1) is in front of
                    // colorkey window (w)
                    w->bBlendWAR = NV_TRUE;
                    w->warWindow = j;
                }
                found = NV_TRUE;
                break;
            }
        }
    }

    /*
     * The alpha blend weights are stored per window overlap condition,
     * including one extra condition for color-key-not-matched.
     *
     * All weights except for the NOKEY ones are relevant to regions that
     * have color key disabled, or color key enabled with key matched.
     *
     * Data is organized in a 2d array as follows:
     *
     * Window X overlapping no other windows:
     *    weight[(1<<X)][X]
     *
     * Window X overlapping window Y:
     *    weight[(1<<X) | (1<<Y)][X]
     *
     * Window X overlapping window Y and Z:
     *    weight[(1<<X) | (1<<Y) | (1<<Z)][X]
     *
     * Window X in regions where color key is enabled, but pixel color is not in
     * color key range:
     *    weight[(1<<NOKEY)][X]
     */

    /* initialize weights for no overlap */
    for( i = 0; i < DC_HAL_WINDOWS; i++ )
    {
        win = &cont->windows[i];

        weight[1 << i][i] = 256;

        DcBlendOperation( win->blend_op, win->alpha_op, win->alpha,
            &weight[1 << i][i], 0, 0 );
    }

    /* initialize weights for key-not-matched condition */
    for( i = 0; i < DC_HAL_WINDOWS; i++ )
    {
        win = &cont->windows[i];

        weight[1 << NOKEY][i] = 256;

        DcBlendOperation( win->blend_op, win->alpha_op, win->alpha,
            &weight[1 << NOKEY][i], 0, 0 );
    }

    /* weights for 2 windows, i is on top of j */
    for( i = 0; i < DC_HAL_WINDOWS; i++ )
    {
        for( j = i + 1; j < DC_HAL_WINDOWS; j++ )
        {
            NvU32 I = order[i];
            NvU32 J = order[j];

            win = &cont->windows[I];

            weight[1<<I | 1<<J][I] = weight[1<<I][I];
            weight[1<<I | 1<<J][J] = weight[1<<J][J];

            DcBlendOperation( win->blend_op, win->alpha_op, weight[1<<I][I],
                &weight[1<<I | 1<<J][I], &weight[1<<I | 1<<J][J], 0 );
        }
    }

    /* selects all of the windows in the weight array */
    mask = (1<<DC_HAL_WINDOWS) - 1;

    /* the bottom window is the last in the order array */
    bottom = (1 << order[DC_HAL_WINDOWS-1]);

    /* weights for all 3 windows */
    for( i = 0; i < DC_HAL_WINDOWS; i++ )
    {
        /* if this is not the bottom window, then take the weight from the
         * 2 window overlap condition, otherwise, take the weight for the
         * no overlap condition.
         */
        if( i != order[DC_HAL_WINDOWS-1] )
        {
            /* not the bottom window:
             *
             * subtracting the bottom window from the mask yields the
             * overlapping windows.
             */
            weight[mask][i] = weight[mask - bottom][i];
        }
        else
        {
            /* this is the bottom window */
            weight[mask][i] = weight[1<<i][i];
        }
    }

    /* use the bottom window as the alpha value */
    win = &cont->windows[order[DC_HAL_WINDOWS-1]];

    DcBlendOperation( win->blend_op, win->alpha_op,
        weight[bottom][order[DC_HAL_WINDOWS-1]],
        &weight[mask][order[0]], &weight[mask][order[1]],
        &weight[mask][order[2]] );

    /* transfer colorkey weight backwards */
    for( i = 0; i < DC_HAL_WINDOWS; i++ )
    {
        if (cont->windows[order[i]].blend_op != NvDdkDispBlendType_ColorKey) {
            continue;
        }

        for( j = i + 1; j < DC_HAL_WINDOWS; j++ )
        {
            int ck = order[i];
            int behind = order[j];
            // Window in front of this one is colorkey.  Absorb weight from
            // ck window into this one.  Weight for the colorkey window should
            // always be zero for key-matched regions.
            // 2WIN
            weight[1<<ck|1<<behind][behind] += weight[1<<ck|1<<behind][ck];
            weight[1<<ck|1<<behind][ck] = 0;
            // 3WIN
            weight[(1<<DC_HAL_WINDOWS)-1][behind] +=
                 weight[(1<<DC_HAL_WINDOWS)-1][ck];
            weight[(1<<DC_HAL_WINDOWS)-1][ck] = 0;
        }
    }

    if (DC_DEBUG_BLEND) {
        NvOsDebugPrintf("Front-Back Ordering: %c %c %c\n", 'A' + order[0],
                                                           'A' + order[1],
                                                           'A' + order[2]);

        NvOsDebugPrintf("Weights    A   B   C \n");
        NvOsDebugPrintf(" 1WIN A:  %3d        \n", weight[1<<0][0]);
        NvOsDebugPrintf(" 1WIN B:      %3d    \n", weight[1<<1][1]);
        NvOsDebugPrintf(" 1WIN C:          %3d\n", weight[1<<2][2]);
        NvOsDebugPrintf(" 2WIN AB: %3d %3d    \n", weight[1<<0|1<<1][0],
                                                   weight[1<<0|1<<1][1]);
        NvOsDebugPrintf(" 2WIN AC: %3d     %3d\n", weight[1<<0|1<<2][0],
                                                   weight[1<<0|1<<2][2]);
        NvOsDebugPrintf(" 2WIN BC:     %3d %3d\n", weight[1<<1|1<<2][1],
                                                   weight[1<<1|1<<2][2]);
        NvOsDebugPrintf(" 3WIN ABC:%3d %3d %3d\n", weight[1<<0|1<<1|1<<2][0],
                                                   weight[1<<0|1<<1|1<<2][1],
                                                   weight[1<<0|1<<1|1<<2][2]);
        NvOsDebugPrintf(" NOKEY:   %3d %3d %3d\n", weight[1<<NOKEY][0],
                                                   weight[1<<NOKEY][1],
                                                   weight[1<<NOKEY][2]);
    }

    /* finally, write the blend registers */
    for( i = 0; i < DC_HAL_WINDOWS; i++ )
    {
        WRITE_NOKEY_BLEND( cont, i, weight[1<<NOKEY][i] );
        WRITE_BLEND( cont, i, 1WIN, weight[1<<i][i] );
        WRITE_BLEND( cont, i, 3WIN_AC, weight[mask][i] );
    }

    WRITE_BLEND( cont, 0, 2WIN_A, weight[1<<0|1<<1][0] );
    WRITE_BLEND( cont, 1, 2WIN_A, weight[1<<0|1<<1][1] );
    WRITE_BLEND( cont, 2, 2WIN_C, weight[1<<2|1<<1][2] );
    WRITE_BLEND( cont, 1, 2WIN_C, weight[1<<2|1<<1][1] );
    WRITE_BLEND( cont, 0, 2WIN_C, weight[1<<0|1<<2][0] );
    WRITE_BLEND( cont, 2, 2WIN_A, weight[1<<0|1<<2][2] );
}

void
DcSetColorKey( DcController *cont, DcWindow *win )
{
    NvU32 val;
    NvU8 r, g, b;
    NvU32 upper;
    NvU32 lower;
    NvU32 reg_upper, reg_lower;

    DC_SAFE( cont, return );

    upper = win->colorkey_upper;
    lower = win->colorkey_lower;

    if( win->colorkey_number == 0 )
    {
        reg_upper = DC_DISP_COLOR_KEY0_UPPER_0;
        reg_lower = DC_DISP_COLOR_KEY0_LOWER_0;
    }
    else
    {
        reg_upper = DC_DISP_COLOR_KEY1_UPPER_0;
        reg_lower = DC_DISP_COLOR_KEY1_LOWER_0;
    }

    /* color key is in A8R8G8B8, use color key0 for DRF */
    b = (NvU8)( upper & 0xff );
    g = (NvU8)((upper & ( 0xff << 8 )) >> 8);
    r = (NvU8)((upper & ( 0xff << 16 )) >> 16);
    val = NV_DRF_NUM( DC_DISP, COLOR_KEY0_UPPER, COLOR_KEY0_U_R, r )
        | NV_DRF_NUM( DC_DISP, COLOR_KEY0_UPPER, COLOR_KEY0_U_G, g )
        | NV_DRF_NUM( DC_DISP, COLOR_KEY0_UPPER, COLOR_KEY0_U_B, b );
    DC_WRITE( cont, reg_upper, val );

    b = (NvU8)( lower & 0xff );
    g = (NvU8)((lower & ( 0xff << 8 )) >> 8);
    r = (NvU8)((lower & ( 0xff << 16 )) >> 16);
    val = NV_DRF_NUM( DC_DISP, COLOR_KEY0_LOWER, COLOR_KEY0_L_R, r )
        | NV_DRF_NUM( DC_DISP, COLOR_KEY0_LOWER, COLOR_KEY0_L_G, g )
        | NV_DRF_NUM( DC_DISP, COLOR_KEY0_LOWER, COLOR_KEY0_L_B, b );
    DC_WRITE( cont, reg_lower, val );
}

void
DcHal_HwTestCrcEnable( NvU32 controller, NvBool enable )
{
    NvU32 val;
    DcController *cont;

    DC_CONTROLLER( controller, cont, return );

    DcRegWrSafe( cont );

    if( enable )
    {
        val = NV_DRF_DEF( DC_COM, CRC_CONTROL, CRC_ENABLE, ENABLE )
            | NV_DRF_DEF( DC_COM, CRC_CONTROL, CRC_INPUT_DATA, ACTIVE_DATA )
            | NV_DRF_DEF( DC_COM, CRC_CONTROL, CRC_ALWAYS, ENABLE );
    }
    else
    {
        val = NV_DRF_DEF( DC_COM, CRC_CONTROL, CRC_ENABLE, DISABLE );
    }

    DC_WRITE( cont, DC_COM_CRC_CONTROL_0, val );
    DcLatch( cont, NV_FALSE );
}

NvU32
DcHal_HwTestCrc( NvU32 controller )
{
    NvU32 val;
    NvU32 reg;
    DcController *cont;
    NvU32 i;

    #define CRC_WAIT_FRAMES 2

    DC_CONTROLLER( controller, cont, return 0 );

    HOST_CLOCK_ENABLE( cont );

    DcRegWrSafe_Direct( cont );

    for( i = 0; i < CRC_WAIT_FRAMES; i++ )
    {
        cont->SyncPointShadow += 2;

        if( NVDDK_DISP_CAP_IS_SET( cont->caps,
            NvDdkDispCapBit_Bug_Double_Syncpt_Incr ) )
        {
            DcLatch( cont, NV_TRUE );
            val = NV_DRF_DEF( DC_CMD, GENERAL_INCR_SYNCPT, GENERAL_COND,
                    REG_WR_SAFE )
                | NV_DRF_NUM( DC_CMD, GENERAL_INCR_SYNCPT, GENERAL_INDX,
                    cont->SyncPointId );
            DC_WRITE_DIRECT( cont, DC_CMD_GENERAL_INCR_SYNCPT_0, val );
            DC_WRITE_DIRECT( cont, DC_CMD_GENERAL_INCR_SYNCPT_0, val );
        }
        else
        {
            val = NV_DRF_DEF( DC_CMD, GENERAL_INCR_SYNCPT, GENERAL_COND,
                    FRAME_START )
                | NV_DRF_NUM( DC_CMD, GENERAL_INCR_SYNCPT, GENERAL_INDX,
                    cont->SyncPointId );
            DC_WRITE_DIRECT( cont, DC_CMD_GENERAL_INCR_SYNCPT_0, val );
            val = NV_DRF_DEF( DC_CMD, GENERAL_INCR_SYNCPT, GENERAL_COND,
                    FRAME_DONE )
                | NV_DRF_NUM( DC_CMD, GENERAL_INCR_SYNCPT, GENERAL_INDX,
                    cont->SyncPointId );
            DC_WRITE_DIRECT( cont, DC_CMD_GENERAL_INCR_SYNCPT_0, val );
        }

        DcLatch( cont, NV_TRUE );
        DC_FLUSH_DIRECT( cont );

        NvRmChannelSyncPointWait( cont->hRm, cont->SyncPointId,
            cont->SyncPointShadow, cont->sem );
    }

    #undef CRC_WAIT_FRAMES

    reg = DC_COM_CRC_CHECKSUM_LATCHED_0 * 4;

    DcRegRd( cont, 1, &reg, &val );

    HOST_CLOCK_DISABLE( cont );

    return val;
}

void
DcHal_HwTestInjectError( NvU32 controller, NvU32 error, NvU32 flags )
{
    DcController *cont;

    DC_CONTROLLER( controller, cont, return );

    switch( error ) {
    case NVDDK_DISP_TEST_ERROR_UNDERFLOW:
        cont->InternalError |= NvDdkDispActiveError_Underflow;
        break;
    case NVDDK_DISP_TEST_ERROR_OVERFLOW:
        cont->InternalError |= NvDdkDispActiveError_Overflow;
        break;
    default:
        NV_ASSERT( !"unknown error" );
        break;
    }

    if( cont->InternalSem && cont->bInternalErrorEnable )
    {
        NvOsSemaphoreSignal( cont->InternalSem );
    }

    cont->bInternalErrorEnable = NV_FALSE;
}
