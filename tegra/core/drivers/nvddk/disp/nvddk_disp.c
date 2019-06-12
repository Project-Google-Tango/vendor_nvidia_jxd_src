/*
 * Copyright (c) 2007-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvddk_disp.h"
#include "nvodm_disp.h"
#include "nvassert.h"
#include "nvos.h"
#include "nvrm_power.h"
#include "nvrm_module.h"
#include "nvrm_pinmux.h"
#include "nvodm_modules.h"
#include "nvddk_disp_structure.h"
#include "nvddk_disp_hw.h"
#include "nvddk_disp_edid.h"
#include "nvrm_analog.h"

static NvDdkDisp *s_Display = NULL;

/** private functions */
static NvBool NvDdkDispPrivAllocMem( NvRmDeviceHandle hDevice,
    NvRmMemHandle *hMem, NvU32 size, NvU32 align );
static NvBool NvDdkDispPrivGetCaps( NvDdkDispHandle hDisplay );
static NvBool NvDdkDispPrivInitState( NvDdkDispHandle hDisplay );
static NvBool NvDdkDispPrivIsSurfaceNull( NvRmSurface *surf );
static NvBool NvDdkDispPrivCompareUsage( NvOdmDispDeviceUsageType ou,
    NvDdkDispDisplayUsageType du );
static NvDdkDispDisplayType NvDdkDispOdmToDdkDeviceType(
    NvOdmDispDeviceType t );
static NvBool NvDdkDispPrivVerifyMode( NvDdkDispControllerHandle hController,
    NvDdkDispMode *pMode );
static void NvDdkDispPrivSanitizeMode( NvDdkDispMode *dm );
static NvBool NvDdkDispPrivIsModeNull( NvOdmDispDeviceMode *m );
static NvBool NvDdkDispPrivModeCompare( NvOdmDispDeviceMode *m1,
    NvOdmDispDeviceMode *m2 );
static NvBool NvDdkDispPrivCommitAllAttributes(
    NvDdkDispControllerHandle hController );
static void NvDdkDispPrivCommitWindow( NvDdkDispWindowHandle hWindow );
static void NvDdkDispPrivCommitDisplay( NvDdkDispDisplayHandle hDisplay );
static void NvDdkDispPrivCommitController( NvDdkDispControllerHandle
    hController );
static NvError NvDdkDispPrivAttachDisplay(
    NvDdkDispControllerHandle hController,
    NvDdkDispDisplayHandle hDisplay, NvU32 flags );
static NvError NvDdkDispPrivDetachDisplay(
    NvDdkDispControllerHandle hController,
    NvDdkDispDisplayHandle hDisplay, NvU32 flags );
static void NvDdkDispBacklightTimeout( void *v );
static void NvDdkDispErrorHandler( void *v );
static void NvDdkDispSmartDimmer( void *v );
static NvBool NvDdkDispPrivEdid( NvDdkDispDisplayHandle hDisplay );
static void NvDdkDispPrivColorKey( NvDdkDispWindowHandle hWindow );
static void NvDdkDispPrivSetupModeFreqs( NvDdkDispPanel *panel );
static void NvDdkDispPrivSetPowerLevel( NvDdkDispDisplayHandle hDisplay,
    NvOdmDispDevicePowerLevel level );
static void NvDdkDispPrivUpdatePanels( NvDdkDispControllerHandle hController,
    NvOdmDispDevicePowerLevel level );
static void NvDdkDispPrivUpdateSurface( NvDdkDispControllerHandle hController,
    NvDdkDispWindowHandle first, NvDdkDispWindowHandle last,
    NvBool *enabled );
static NvBool NvDdkDispPrivModeCommit( NvDdkDispControllerHandle hController );
static NvBool NvDdkDispPrivPowerHint( NvDdkDispHandle hDisp );
static void NvDdkDispPrivPrintMode( NvOdmDispDeviceMode *om );
static void NvDdkDispPrivPrintSurface( NvRmSurface *surf );
static NvS32 NvDdkDispPrivGetMaxFrequency( NvDdkDispDisplayHandle hDisplay );

NvError
NvDdkDispOpen( NvRmDeviceHandle hDevice,
    NvDdkDispHandle *hDisp,
    NvU32 flags)
{
    NvError err;
    NvDdkDispHandle h;
    NvBool b;

    NV_ASSERT( hDevice );
    NV_ASSERT( hDisp );
    NV_ASSERT( flags == 0 );

    // FIXME: how to make thread-safe?
    if( s_Display )
    {
        NvOsMutexLock( s_Display->mutex );
        s_Display->refcount++;
        *hDisp = s_Display;
        NvOsMutexUnlock( s_Display->mutex );
        return NvSuccess;
    }
    s_Display = NvOsAlloc(sizeof(NvDdkDisp));
    if (!s_Display)
        goto fail;
    NvOsMemset(s_Display, 0x0,  sizeof(NvDdkDisp));
    h = s_Display;

    err = NvOsMutexCreate(&h->mutex);
    if( err != NvSuccess )
    {
        goto fail;
    }

    h->refcount = 1;
    h->hDevice = hDevice;

    b = NvDdkDispPrivGetCaps( h );
    if( !b )
    {
        goto fail;
    }

    /* set default attributes, etc. */
    b = NvDdkDispPrivInitState( h );
    if( !b )
    {
        goto fail;
    }

    *hDisp = h;

    return NvSuccess;

fail:
    return NvError_DispInitFailed;
}

void
NvDdkDispClose( NvDdkDispHandle hDisp, NvU32 flags )
{
    NvBool bDestroy = NV_FALSE;

    NV_ASSERT( flags == 0 );

    if( !hDisp )
    {
        return;
    }

    NvOsMutexLock( hDisp->mutex );

    hDisp->refcount--;
    if( !hDisp->refcount )
    {
        NvU32 i, j, attached;
        NvDdkDispController *cont;
        NvDdkDispDisplay *disp;

        bDestroy = NV_TRUE;

        for( i = 0; i < NVDDK_DISP_MAX_CONTROLLERS; i++ )
        {
            cont = &hDisp->controllers[i];

            /* detach the displays -- downstream displays (hdmi, tv) need to be
             * disabled
             */
            attached = cont->nAttached;
            for( j = 0; j < attached; j++ )
            {
                disp = cont->AttachedDisplays[j];
                NvDdkDispAttachDisplay( 0, disp, 0 );
            }

            if( attached )
            {
                /* shutdown the hardware */
                NV_ASSERT_SUCCESS(
                    NvDdkDispSetMode( cont, 0, 0 )
                );
            }

            if( cont->timeout_thread && cont->timeout_thread->thread )
            {
                cont->timeout_thread->bShutdown = NV_TRUE;
                NvOsSemaphoreSignal( cont->timeout_thread->sem );
                NvOsThreadJoin( cont->timeout_thread->thread );
                NvOsSemaphoreDestroy( cont->timeout_thread->sem );
            }

            if( cont->error_thread && cont->error_thread->thread )
            {
                cont->error_thread->bShutdown = NV_TRUE;
                NvOsSemaphoreSignal( cont->error_thread->sem );
                NvOsThreadJoin( cont->error_thread->thread );
                NvOsSemaphoreDestroy( cont->error_thread->sem );
            }

            if( cont->smartdimmer_thread && cont->smartdimmer_thread->thread )
            {
                cont->smartdimmer_thread->bShutdown = NV_TRUE;
                NvOsSemaphoreSignal( cont->smartdimmer_thread->sem );
                NvOsThreadJoin( cont->smartdimmer_thread->thread );
                NvOsSemaphoreDestroy( cont->smartdimmer_thread->sem );
            }

            NvOsFree( cont->timeout_thread );
            NvOsFree( cont->error_thread );
            NvOsFree( cont->smartdimmer_thread );

            NvOsMutexDestroy( cont->mutex );
            NvRmMemHandleFree( cont->cursor.hMem1 );
            NvRmMemHandleFree( cont->cursor.hMem2 );
        }
        for( i = 0; i < NVDDK_DISP_MAX_DISPLAYS; i++ )
        {
            disp = &hDisp->displays[i];
            NvOsMutexDestroy( disp->mutex );
        }

        NvOdmDispReleaseDevices( hDisp->nOdmPanels, hDisp->hPanels );
        hDisp->nPanels = 0;
        hDisp->nOdmPanels = 0;
    }

    NvOsMutexUnlock( hDisp->mutex );

    if( bDestroy )
    {
        NvOsMutexDestroy( hDisp->mutex );
        NvOsFree(hDisp);
        s_Display = NULL;
    }
}

/* timeout thread function */
static void
NvDdkDispBacklightTimeout( void *v )
{
    NvError err;
    NvBool b;
    NvDdkDispTimeoutThread *t = v;

    NV_ASSERT( t );
    NV_ASSERT( t->sem );

    t->timeout = NV_WAIT_INFINITE;

    while( !t->bShutdown )
    {
        err = NvOsSemaphoreWaitTimeout( t->sem, t->timeout );
        if( err == NvError_Timeout )
        {
            /* shut off backlight */
            NvOsMutexLock( t->hController->mutex );

            if( t->hController->state == ControllerState_Active )
            {
                t->hal->HwBegin( t->hController->Index );
                // FIXME: get panel
                t->hal->HwSetBacklight( t->hController->Index, 0 );
                b = t->hal->HwEnd( t->hController->Index, 0 );
                (void)b;
                NV_ASSERT( b );
            }

            NvOsMutexUnlock( t->hController->mutex );

            t->timeout = NV_WAIT_INFINITE;
        }
    }
}

/* smart dimmer thread */
static NvBool
NvDdkDispPrivCreateSmartDimmerThread( NvDdkDispControllerHandle hController )
{
    NvError e;
    NvDdkDispSmartDimmerThread *t = 0;
    t = NvOsAlloc( sizeof(NvDdkDispSmartDimmerThread) );
    if( !t )
    {
        goto fail;
    }

    NvOsMemset( t, 0, sizeof(NvDdkDispSmartDimmerThread) );
    t->hController = hController;
    t->bShutdown = NV_FALSE;
    t->hal = &hController->hal;
    e = NvOsSemaphoreCreate( &t->sem, 0 );
    if( e != NvSuccess )
    {
        goto fail;
    }

    hController->smartdimmer_thread = t;
    e = NvOsThreadCreate( NvDdkDispSmartDimmer, t, &t->thread );
    if( e != NvSuccess )
    {
        goto fail;
    }

    return NV_TRUE;
fail:
    NvOsFree( t );
    return NV_FALSE;
}

static void
NvDdkDispSmartDimmer( void *v )
{
    NvError err;
    NvU32 timeout;
    NvDdkDispSmartDimmerThread *t = v;
    NvDdkDispDisplayHandle hDisplay;
    NvDdkDispPanel *panel;
    NvU8 intensity;
    NvU8 brightness, prevbrightness = 0;

    #define NVDDK_DISP_SMART_DIMMER_TIMEOUT 33

    NV_ASSERT( t );
    NV_ASSERT( t->sem );

    // FIXME: just wait one frame worth of time. should tune this and
    // synchronize with vblank, etc.
    timeout = NVDDK_DISP_SMART_DIMMER_TIMEOUT;

    while( !t->bShutdown )
    {
        err = NvOsSemaphoreWaitTimeout( t->sem, timeout );
        if( err != NvError_Timeout )
        {
            timeout = NVDDK_DISP_SMART_DIMMER_TIMEOUT;
            continue;
        }

        // Check the thread shutdown after we get the sem signal!
        if (t->bShutdown)
        {
            break;
        }

        NvOsMutexLock( t->hController->mutex );
        if( t->hController->state != ControllerState_Active ||
            t->hController->attr.SmartDimmer_Enable == 0 )
        {
            timeout = NV_WAIT_INFINITE;
            NvOsMutexUnlock( t->hController->mutex );
            t->hal->HwSmartDimmerLog( t->hController->Index, 0, 0, NV_TRUE );
            continue;
        }

        // smart dimmer only really supports 1 attached display
        hDisplay = t->hController->AttachedDisplays[0];

        // Switch over to the display's mutex
        NvOsMutexUnlock( t->hController->mutex );
        NvOsMutexLock( hDisplay->mutex );
        // And make sure the display's hal matches the thread's, just in case.
        NV_ASSERT ( &hDisplay->CurrentController->hal == t->hal );

        panel = hDisplay->panel;

        if( hDisplay->attr.Backlight != NvDdkDispBacklightControl_On )
        {
             continue;
        }

        t->hal->HwSmartDimmerBrightness( t->hController->Index, &brightness );

        if (brightness && (brightness != prevbrightness))
        {
            intensity = hDisplay->attr.InBacklightIntensity;
            prevbrightness = brightness;
            intensity = (NvU8)(((NvU32)brightness * (NvU32)intensity) / 255);

            t->hal->HwBegin(hDisplay->CurrentController->Index);
            t->hal->HwSetBacklight(hDisplay->CurrentController->Index, intensity);
            if (!t->hal->HwEnd(hDisplay->CurrentController->Index, 0))
                NV_ASSERT(!"NvDdkDispSmartDimmer: HwEnd() failed");
            if (panel->hPanel)
                NvOdmDispSetBacklight(panel->hPanel, intensity);
            hDisplay->attr.OutBacklightIntensity = intensity;
            t->hal->HwSmartDimmerLog(hDisplay->CurrentController->Index,
                    hDisplay->attr.InBacklightIntensity,
                    hDisplay->attr.OutBacklightIntensity,
                    NV_FALSE);
        }
        NvOsMutexUnlock(hDisplay->mutex);
    }
    #undef NVDDK_DISP_SMART_DIMMER_TIMEOUT
}

/* error handler thread and helpers */
static NvU32
NvDdkDispPrivGetLastError( NvDdkDispErrorThread *t )
{
    NvU32 error = 0;

    NvOsMutexLock( t->hController->mutex );

    if( t->hController->state != ControllerState_Active )
    {
        goto clean;
    }

    error = t->hal->HwGetLastError( t->hController->Index );

clean:
    NvOsMutexUnlock( t->hController->mutex );
    return error;
}

static void
NvDdkDispPrivReset( NvDdkDispControllerHandle hController )
{
    NvOsMutexLock( hController->mutex );

    /* make sure display is clocked */
    NV_ASSERT_SUCCESS(
        NvRmPowerModuleClockControl( hController->hDisp->hDevice,
            NVRM_MODULE_ID( NvRmModuleID_Display, hController->Index ),
            hController->hDisp->PowerClientId, NV_TRUE )
    );

    /* reset the hardware */
    if( hController->state == ControllerState_Active )
    {
        NvDdkDispControllerSuspend( hController, 0 );

        NvRmModuleReset( hController->hDisp->hDevice,
            NVRM_MODULE_ID( NvRmModuleID_Display, hController->Index ));

        NvDdkDispControllerResume( hController, 0 );
    }
    else
    {
        NvRmModuleReset( hController->hDisp->hDevice,
            NVRM_MODULE_ID( NvRmModuleID_Display, hController->Index ));
    }

    NV_ASSERT_SUCCESS(
        NvRmPowerModuleClockControl( hController->hDisp->hDevice,
            NVRM_MODULE_ID( NvRmModuleID_Display, hController->Index ),
            hController->hDisp->PowerClientId, NV_FALSE )
    );

    NvOsMutexUnlock( hController->mutex );
}

static void
NvDdkDispPrivSetErrorSem( NvDdkDispErrorThread *t )
{
    NvBool b;

    NvOsMutexLock( t->hController->mutex );

    if( t->hController->state != ControllerState_Active )
    {
        goto clean;
    }

    t->hal->HwBegin( t->hController->Index );
    t->hal->HwSetErrorSem( t->hController->Index, t->sem );
    b = t->hal->HwEnd( t->hController->Index, 0 );
    (void)b;
    NV_ASSERT( b );

clean:
    NvOsMutexUnlock( t->hController->mutex );
}

static void
NvDdkDispErrorHandler( void *v )
{
    NvU32 error;
    NvU32 timeout;
    NvDdkDispErrorThread *t = v;
    NvU32 count;
    NvBool bUnderflow = NV_FALSE;

    #define DEFAULT_TIMEOUT 500
    #define MAX_TIMEOUT 5000
    #define MAX_UNDERFLOWS 4
    #define ERROR_BITS (NvDdkDispActiveError_Underflow | \
                        NvDdkDispActiveError_Overflow)

    NV_ASSERT( t );
    NV_ASSERT( t->sem );

    timeout = DEFAULT_TIMEOUT;

    count = 0;
    while( !t->bShutdown )
    {
        NvOsSemaphoreWait( t->sem );
        if( t->bShutdown )
        {
            break;
        }

        error = NvDdkDispPrivGetLastError( t );
        if( !(error & ERROR_BITS) )
        {
            NvDdkDispPrivSetErrorSem( t );
            continue;
        }

        count++;
        bUnderflow = NV_TRUE;
        while( bUnderflow )
        {
            /* wait until underflows stop or the maximum underflow count is
             * reached.
             */
            NvOsSleepMS( timeout );
            if( t->bShutdown )
            {
                break;
            }

            error = NvDdkDispPrivGetLastError( t );
            if( error & ERROR_BITS )
            {
                count++;
            }

            if( count < MAX_UNDERFLOWS && (error & ERROR_BITS) )
            {
                /* exponential backoff */
                timeout = NV_MIN( timeout * 2, MAX_TIMEOUT );
            }
            else
            {
                // FIXME: need to figure out why display underruns during boot.
                // ignore one underflow.
                if( count > 1 )
                {
                    NvDdkDispPrivReset( t->hController );
                }

                bUnderflow = NV_FALSE;
                timeout = DEFAULT_TIMEOUT;
                // ignore the first underflow only
                count = 1;
            }
        }

        /* re-enable error handler */
        NvDdkDispPrivSetErrorSem( t );
    }

    #undef DEFAULT_TIMEOUT
    #undef MAX_TIMEOUT
    #undef MAX_UNDERFLOWS
    #undef ERROR_BITS
}

static NvBool
NvDdkDispPrivAllocMem( NvRmDeviceHandle hDevice, NvRmMemHandle *hMem,
    NvU32 size, NvU32 align )
{
    NvError err;

    NV_ASSERT( hMem );

    err = NvRmMemHandleAlloc(hDevice, NULL, 0, align,
            NvOsMemAttribute_Uncached, size,
            NVRM_MEM_TAG_DISPLAY_MISC, 1, hMem);
    if( err != NvSuccess )
    {
        goto fail;
    }

    return NV_TRUE;

fail:
    NvRmMemHandleFree( *hMem );
    return NV_FALSE;
}

static NvBool
NvDdkDispPrivGetCaps( NvDdkDispHandle hDisp )
{
    NvError err;
    // Disp Capabilities. _s = sim, _e = emu.
    static NvDdkDispCapabilities s_cap_ap15;
    static NvDdkDispCapabilities s_cap_ap20;
    static NvDdkDispCapabilities s_cap_t30;
    static NvDdkDispCapabilities s_cap_t11x;
    static NvDdkDispCapabilities s_cap_t12x;
    static NvDdkDispCapabilities s_cap_t12x_e;
    NvDdkDispCapabilities *cap;
    static NvRmModuleCapability s_caps[] =
        { { 1, 4, 0, NvRmModulePlatform_Silicon, &s_cap_t30 },
          { 1, 5, 0, NvRmModulePlatform_Silicon, &s_cap_t11x },
          { 1, 7, 0, NvRmModulePlatform_Silicon, &s_cap_t12x },
          { 1, 7, 0, NvRmModulePlatform_Emulation, &s_cap_t12x_e },
        };

    static NvDdkDispWindowCap s_dc_win_cap[3];
    static NvDdkDispWindowCap s_dc_win_cap1[3];
    static NvDdkDispWindowCap s_dc_win_cap2[3];

    static NvColorFormat s_dc_formats0[] =
        {
            NvColorFormat_A4R4G4B4,
            NvColorFormat_A1R5G5B5,
            // FIXME: can support this with byte-swapping?
            //NvColorFormat_R5G5B5A1,
            NvColorFormat_R5G6B5,
            NvColorFormat_A8B8G8R8,
            NvColorFormat_A8R8G8B8,
            NvColorFormat_B8G8R8A8,
            NvColorFormat_R8G8B8A8,
            NvColorFormat_X8R8G8B8,
            (NvColorFormat)0,
        };
    static NvColorFormat s_dc_formats1[] =
        {
            NvColorFormat_A4R4G4B4,
            NvColorFormat_A1R5G5B5,
            // FIXME: can support this with byte-swapping?
            //NvColorFormat_R5G5B5A1,
            NvColorFormat_R5G6B5,
            NvColorFormat_A8B8G8R8,
            NvColorFormat_A8R8G8B8,
            NvColorFormat_B8G8R8A8,
            NvColorFormat_R8G8B8A8,
            NvColorFormat_X8R8G8B8,
            NvColorFormat_VYUY,
            NvColorFormat_YVYU,
            NvColorFormat_UYVY,
            NvColorFormat_YUYV,
            NvColorFormat_Y8,
            NvColorFormat_U8,
            NvColorFormat_V8,
            (NvColorFormat)0,
        };
    static NvYuvColorFormat s_dc_yuv_formats1[] =
        {
            NvYuvColorFormat_YUV422,
            NvYuvColorFormat_YUV422R,
            NvYuvColorFormat_YUV420,
            (NvYuvColorFormat)0,
        };

    s_dc_win_cap[0].formats = s_dc_formats0;
    s_dc_win_cap[1].formats = s_dc_formats1;
    s_dc_win_cap[2].formats = s_dc_formats1;
    s_dc_win_cap[0].yuv_formats = 0;
    s_dc_win_cap[1].yuv_formats = s_dc_yuv_formats1;
    s_dc_win_cap[2].yuv_formats = s_dc_yuv_formats1;

    s_dc_win_cap[0].bCsc = NV_FALSE;
    s_dc_win_cap[0].bHorizFilter = NV_FALSE;
    s_dc_win_cap[0].bVertFilter = NV_FALSE;
    s_dc_win_cap[1].bCsc = NV_TRUE;
    s_dc_win_cap[1].bHorizFilter = NV_TRUE;
    s_dc_win_cap[1].bVertFilter = NV_TRUE;
    s_dc_win_cap[2].bCsc = NV_TRUE;
    s_dc_win_cap[2].bHorizFilter = NV_TRUE;
    s_dc_win_cap[2].bVertFilter = NV_FALSE;

    s_dc_win_cap1[0] = s_dc_win_cap[0];
    s_dc_win_cap1[1] = s_dc_win_cap[1];
    s_dc_win_cap1[2] = s_dc_win_cap[2];
    s_dc_win_cap2[0] = s_dc_win_cap[0];
    s_dc_win_cap2[1] = s_dc_win_cap[1];
    s_dc_win_cap2[2] = s_dc_win_cap[2];

    /* setup Premultiplied Alpha support */
    s_dc_win_cap[0].bPremultipliedAlpha =
        s_dc_win_cap[1].bPremultipliedAlpha =
        s_dc_win_cap[2].bPremultipliedAlpha = NV_FALSE;
    s_dc_win_cap1[0].bPremultipliedAlpha = NV_TRUE;
    s_dc_win_cap1[1].bPremultipliedAlpha =
        s_dc_win_cap1[2].bPremultipliedAlpha = NV_FALSE;
    s_dc_win_cap2[0].bPremultipliedAlpha =
        s_dc_win_cap2[1].bPremultipliedAlpha =
        s_dc_win_cap2[2].bPremultipliedAlpha = NV_TRUE;

    /* ap15 and beyond could have different number of DC instances */
    s_cap_ap15.NumControllers = NvRmModuleGetNumInstances( hDisp->hDevice,
        NvRmModuleID_Display );
    NV_ASSERT( NVDDK_DISP_MAX_DISPLAYS >= 5 );
    s_cap_ap15.NumDisplays = 5;
    s_cap_ap15.NumWindows = 3;
    s_cap_ap15.engine = NvDdkDispEngine_Dc;
    s_cap_ap15.HardwareId = 1;
    NVDDK_DISP_CAP_SET( &s_cap_ap15, NvDdkDispCapBit_Cursor_x32 );
    NVDDK_DISP_CAP_SET( &s_cap_ap15, NvDdkDispCapBit_Cursor_x64 );
    NVDDK_DISP_CAP_SET( &s_cap_ap15, NvDdkDispCapBit_Bug_363177 );
    NVDDK_DISP_CAP_SET( &s_cap_ap15, NvDdkDispCapBit_Bug_BandwidthPowerHint );
    NVDDK_DISP_CAP_SET( &s_cap_ap15, NvDdkDispCapBit_Bug_UnderflowReset );
    NVDDK_DISP_CAP_SET( &s_cap_ap15, NvDdkDispCapBit_Bug_Double_Syncpt_Incr );
    NVDDK_DISP_CAP_SET( &s_cap_ap15, NvDdkDispCapBit_LineBuffer_1280 );
    NVDDK_DISP_CAP_CLEAR( &s_cap_ap15, NvDdkDispCapBit_Hdtv_Analog );
    NVDDK_DISP_CAP_CLEAR( &s_cap_ap15, NvDdkDispCapBit_Hdmi_Kfuse );
    NVDDK_DISP_CAP_CLEAR( &s_cap_ap15, NvDdkDispCapBit_Hdmi_PreEmphasis );
    NVDDK_DISP_CAP_CLEAR( &s_cap_ap15, NvDdkDispCapBit_SmartDimmer );
    NVDDK_DISP_CAP_CLEAR( &s_cap_ap15, NvDdkDispCapBit_Bug_722394 );
    NVDDK_DISP_CAP_CLEAR( &s_cap_ap15, NvDdkDispCapBit_ForceDefaultExternalDisplayMode );
    NVDDK_DISP_CAP_CLEAR( &s_cap_ap15, NvDdkDispCapBit_RatioCveTvoFreq_1_2 );

    /* just in case we need pushbuffer access to display */
    NVDDK_DISP_CAP_CLEAR( &s_cap_ap15, NvDdkDispCapBit_IndirectRegs );

    /* need 2 controllers for removable displays */
    if( s_cap_ap15.NumControllers > 1 )
    {
        NVDDK_DISP_CAP_SET( &s_cap_ap15, NvDdkDispCapBit_Hdmi );
        NVDDK_DISP_CAP_SET( &s_cap_ap15, NvDdkDispCapBit_Tv );
        NVDDK_DISP_CAP_SET( &s_cap_ap15, NvDdkDispCapBit_Crt );
    }
    else
    {
        NVDDK_DISP_CAP_CLEAR( &s_cap_ap15, NvDdkDispCapBit_Hdmi );
        NVDDK_DISP_CAP_CLEAR( &s_cap_ap15, NvDdkDispCapBit_Tv );
        NVDDK_DISP_CAP_CLEAR( &s_cap_ap15, NvDdkDispCapBit_Crt );
    }

    /* hardware version numbers:
     * 1.4: t30
     * 1.5: t11x
     * 1.7: t12x
     */

    /* 1.3, ap20 is way cool, has new features:
     *  - new tv out controller, cve5, which supports analog hdtv
     *  - hdmi kfuse, random tmds macro changes (for the better, of course)
     *  - hdmi pre-emphasis for 1080p
     *  - no smart dimmer - hardware is hopelessly broken
     */
    s_cap_ap20 = s_cap_ap15;
    NVDDK_DISP_CAP_SET( &s_cap_ap20, NvDdkDispCapBit_Bug_722394 );
    NVDDK_DISP_CAP_SET( &s_cap_ap20, NvDdkDispCapBit_Hdtv_Analog );
    NVDDK_DISP_CAP_SET( &s_cap_ap20, NvDdkDispCapBit_Hdmi_Kfuse );
    NVDDK_DISP_CAP_SET( &s_cap_ap20, NvDdkDispCapBit_Hdmi_PreEmphasis );
    NVDDK_DISP_CAP_CLEAR( &s_cap_ap20, NvDdkDispCapBit_SmartDimmer );
    NVDDK_DISP_CAP_CLEAR( &s_cap_ap20, NvDdkDispCapBit_Bug_UnderflowReset );
    NVDDK_DISP_CAP_SET( &s_cap_ap20,
        NvDdkDispCapBit_Bug_SmartDimmer_AutomaticMode );
    NVDDK_DISP_CAP_SET( &s_cap_ap20,
        NvDdkDispCapBit_Bug_Disable_Underflow_Line_Flush );

    s_cap_ap15.WindowCaps = s_dc_win_cap;
    s_cap_ap20.WindowCaps = s_dc_win_cap2;

    /* t30:
     * - smartdimmer registers changed - smartdimer hw should work this time
     */
    s_cap_t30 = s_cap_ap20;
    NVDDK_DISP_CAP_SET( &s_cap_t30, NvDdkDispCapBit_SmartDimmer );
    NVDDK_DISP_CAP_SET( &s_cap_t30, NvDdkDispCapBit_Bug_SmartDimmer_Histogram_Reset );
    NVDDK_DISP_CAP_CLEAR( &s_cap_t30, NvDdkDispCapBit_ForceDefaultExternalDisplayMode );
    NVDDK_DISP_CAP_SET( &s_cap_t30, NvDdkDispCapBit_RatioCveTvoFreq_1_2 );

    s_cap_t11x = s_cap_t30;
    s_cap_t12x = s_cap_t30;

    /* emulation caps */
    s_cap_t12x_e = s_cap_t30;
    NVDDK_DISP_CAP_SET( &s_cap_t12x_e, NvDdkDispCapBit_Dsi_ForcePclkDiv1 );
    NVDDK_DISP_CAP_SET( &s_cap_t12x_e, NvDdkDispCapBit_ForceDefaultExternalDisplayMode );

    err = NvRmModuleGetCapabilities( hDisp->hDevice, NvRmModuleID_Display,
        s_caps, NV_ARRAY_SIZE(s_caps), (void **)&cap );
    if( err == NvSuccess )
    {
        hDisp->caps = (NvDdkDispCapabilities *)cap;
        return NV_TRUE;
    }
    else
    {
        return NV_FALSE;
    }
}

static NvBool
NvDdkDispPrivCompareUsage( NvOdmDispDeviceUsageType ou,
    NvDdkDispDisplayUsageType du )
{
    switch( ou ) {
    case NvOdmDispDeviceUsage_Primary:
        return ( du == NvDdkDispDisplayUsage_Primary );
    case NvOdmDispDeviceUsage_Secondary:
        return ( du == NvDdkDispDisplayUsage_Secondary );
    case NvOdmDispDeviceUsage_Removable:
        return ( du == NvDdkDispDisplayUsage_Removable );
    case NvOdmDispDeviceUsage_Unknown:
        return ( du == NvDdkDispDisplayUsage_Unknown );
    default:
        NV_ASSERT( !"bad usage" );
        return NV_FALSE;
    }
}

static NvDdkDispDisplayType
NvDdkDispOdmToDdkDeviceType( NvOdmDispDeviceType t )
{
    switch( t ) {
    case NvOdmDispDeviceType_TFT:
        return NvDdkDispDisplayType_TFTLCD;
    case NvOdmDispDeviceType_CLI:
        return NvDdkDispDisplayType_CLILCD;
    case NvOdmDispDeviceType_DSI:
        return NvDdkDispDisplayType_DSI;
    case NvOdmDispDeviceType_CRT:
        return NvDdkDispDisplayType_CRT;
    case NvOdmDispDeviceType_TV:
        return NvDdkDispDisplayType_TV;
    case NvOdmDispDeviceType_HDMI:
        return NvDdkDispDisplayType_HDMI;
    default:
        NV_ASSERT( !"bad device type" );
        return (NvDdkDispDisplayType)0;
    }
}

static void
NvDdkDispPrivSetupModeFreqs( NvDdkDispPanel *panel )
{
    NvOdmDispDeviceTiming *timing;
    NvU32 panel_freq = 0;
    NvU32 refresh_freq = 0;
    NvU32 h_total, v_total;
    NvU32 i;

    /* calculate frequencies */
    for( i = 0; i < panel->nModes; i++ )
    {
        NvOdmDispDeviceMode *mode = &panel->modes[i];
        timing = &mode->timing;

        h_total = timing->Horiz_BackPorch + timing->Horiz_SyncWidth +
            timing->Horiz_DispActive + timing->Horiz_FrontPorch;
        v_total = timing->Vert_BackPorch + timing->Vert_SyncWidth +
            timing->Vert_DispActive + timing->Vert_FrontPorch;

        if( (!mode->refresh && !mode->frequency) || !h_total || !v_total  )
        {
            /* if neither refresh or frequency are given, then calculate both
             * assuming 60hz
             */
            mode->refresh = (60 << 16);
        }
        else if( !mode->refresh )
        {
            /* frequency is given, figure out what the refresh has to be to
             * meet timing.
             *
             * convert to Hz, divide out the blanking, then convert to Q16.
             */
            mode->refresh = (mode->frequency * 1000) / h_total / v_total;
            mode->refresh <<= 16;
        }

        if( !mode->frequency )
        {
            /* multiply by refresh fps, refresh is in 15.16 (hertz) */
            refresh_freq = ( mode->refresh >> 16 );
            panel_freq = ( h_total * v_total * refresh_freq );

            /* convert to khz */
            panel_freq /= 1000;

            mode->frequency = panel_freq;
        }
    }
}

static void
NvDdkDispPrivDebugPanelSetup( NvDdkDispPanel *p )
{
    char *type, *usage;

    NvOsDebugPrintf( "display device/ " );

    switch( p->Usage ) {
    case NvOdmDispDeviceUsage_Primary:
        usage = "Primary";
        break;
    case NvOdmDispDeviceUsage_Secondary:
        usage = "Secondary";
        break;
    case NvOdmDispDeviceUsage_Removable:
        usage = "Removable";
        break;
    default:
        NV_ASSERT( !"unknown usage type" );
        usage = "unknown";
        break;
    }

    switch( p->Type ) {
    case NvOdmDispDeviceType_TFT:
        type = "TFT";
        break;
    case NvOdmDispDeviceType_CLI:
        type = "CLI";
        break;
    case NvOdmDispDeviceType_DSI:
        type = "DSI";
        break;
    case NvOdmDispDeviceType_CRT:
        type = "CRT";
        break;
    case NvOdmDispDeviceType_TV:
        type = "TV";
        break;
    case NvOdmDispDeviceType_HDMI:
        type = "HDMI";
        break;
    default:
        NV_ASSERT( !"unknown display type" );
        type = "unknown";
        break;
    }

    NvOsDebugPrintf( "type: %s usage: %s\n", type, usage );
}

static NvBool
NvDdkDispPrivSetupPanels( NvDdkDispHandle hDisp )
{
    NvBool err;
    NvDdkDispDisplay *disp;
    NvDdkDispCapabilities *caps;
    NvDdkDispHal *hal;
    NvU32 i, j;
    NvBool bHdmiPrimary = NV_FALSE;
    NvBool bCrtPrimary = NV_FALSE;

    caps = hDisp->caps;
    hDisp->nPanels = 0;
    hDisp->nOdmPanels = 0;
    hal = &hDisp->controllers[0].hal;

    #define NVDDK_DISP_DEBUG_PANEL_SETUP (0)

    /* setup usage: the first display is the primary, the second is the
     * secondary, everything else is removable.
     */
    disp = &hDisp->displays[0];
    disp->attr.Usage = NvDdkDispDisplayUsage_Primary;
    if( caps->NumDisplays > 1 )
    {
        disp++;
        disp->attr.Usage = NvDdkDispDisplayUsage_Secondary;
    }
    for( i = 2; i < caps->NumDisplays; i++ )
    {
        disp++;
        disp->attr.Usage = NvDdkDispDisplayUsage_Removable;
    }

    /* get the panels */
    NvOdmDispListDevices( &hDisp->nPanels, 0 );
    NvOdmDispListDevices( &hDisp->nPanels, hDisp->hPanels );
    hDisp->nOdmPanels = hDisp->nPanels;

    #define GET_ODM_ATTR( panel, attr ) \
        err = NvOdmDispGetParameter( (panel)->hPanel, \
            NvOdmDispParameter_##attr, (NvU32 *)&(panel)->attr );\
        if( !err ) \
        { \
            return NV_FALSE; \
        }

    #define GET_SUPPORTED_ODM_ATTR( panel, attr ) \
        err = NvOdmDispGetParameter( (panel)->hPanel, \
            NvOdmDispParameter_##attr, (NvU32 *)&(panel)->attr );\
        if( !err ) \
        { \
            (panel)->attr = 0x0; \
        }

    for( i = 0; i < hDisp->nPanels; i++ )
    {
        NvDdkDispPanel *p;

        p = &hDisp->panels[i];
        p->hPanel = hDisp->hPanels[i];
        p->DeviceIndex = i;
    }

    for( i = 0; i < hDisp->nPanels; i++ )
    {
        NvDdkDispPanel *p = &hDisp->panels[i];

        GET_ODM_ATTR( p, Type );

        /* setup the dsi transport, if necessary */
        if( p->Type == NvOdmDispDeviceType_DSI )
        {
            /* it's important to setup the dsi transport as soon as possible
             * so the adaptation has a chance to query the panel before
             * getting the rest of the attributes.
             */
            err = NvOdmDispSetSpecialFunction( p->hPanel,
                NvOdmDispSpecialFunction_DSI_Transport, hal->DsiTransport );
            if( !err )
            {
                return NV_FALSE;
            }

            /* need to track that there's a DSI panel in the system. if so,
             * the MIPI flag must only be passed into the clock source control
             * api for the DSI panel since the MIPI PLL is the only PLL that
             * can drive the panel. If there is no DSI panel, then HDMI can use
             * the MIPI flag to as an alternate source.
             */
            hDisp->bDsiPresent = NV_TRUE;
        }

        /* get the panel modes */
        NvOdmDispListModes( p->hPanel, &p->nModes, 0 );
        p->nModes = NV_MIN( NVDDK_DISP_MAX_MODES, p->nModes );
        NvOdmDispListModes( p->hPanel, &p->nModes, p->modes );

        /* get panel attributes */
        GET_ODM_ATTR( p, Usage )
        GET_ODM_ATTR( p, MaxHorizontalResolution );
        GET_ODM_ATTR( p, MaxVerticalResolution );
        GET_ODM_ATTR( p, BaseColorSize );
        GET_ODM_ATTR( p, DataAlignment );
        GET_ODM_ATTR( p, PinMap );

        if (hDisp->bDsiPresent)
            GET_ODM_ATTR( p, DsiInstance );

        GET_SUPPORTED_ODM_ATTR( p, ColorCalibrationRed );
        GET_SUPPORTED_ODM_ATTR( p, ColorCalibrationGreen );
        GET_SUPPORTED_ODM_ATTR( p, ColorCalibrationBlue );
        GET_ODM_ATTR( p, PwmOutputPin );
        GET_SUPPORTED_ODM_ATTR( p, BacklightInitialIntensity );

        /* get the extended config */
        p->Config = NvOdmDispGetConfiguration( p->hPanel );

        /* check for external displays as a primary display */
        if( p->Usage == NvOdmDispDeviceUsage_Primary )
        {
            if( p->Type == NvOdmDispDeviceType_HDMI )
            {
                // instantiate the hdmi hal here
                bHdmiPrimary = NV_TRUE;
                p->hPanel = 0; // no ODM adaptation
                p->Config = 0;
                p->builtin = hal->Hdmi;
            }
            else if( p->Type == NvOdmDispDeviceType_CRT )
            {
                bCrtPrimary = NV_TRUE;
                p->hPanel = 0; // no ODM adaptation
                p->Config = 0;
                p->builtin = hal->Crt;
            }
        }

        if( NVDDK_DISP_DEBUG_PANEL_SETUP )
        {
            NvDdkDispPrivDebugPanelSetup( p );
        }

        /* calculate mode frequencies */
        if( p->hPanel )
        {
            NvDdkDispPrivSetupModeFreqs( p );
        }
    }

    /* add the built-in display devices */
    if( hal->Tv )
    {
        NvDdkDispPanel *p = &hDisp->panels[hDisp->nPanels];
        hDisp->nPanels++;

        p->hPanel = 0; // no ODM adaptation
        p->Config = 0;
        p->builtin = hal->Tv;
    }
    if( hal->Crt && !bCrtPrimary )
    {
        NvDdkDispPanel *p = &hDisp->panels[hDisp->nPanels];
        hDisp->nPanels++;

        p->hPanel = 0; // no ODM adaptation
        p->Config = 0;
        p->builtin = hal->Crt;
    }
    if( hal->Hdmi && !bHdmiPrimary )
    {
        NvDdkDispPanel *p = &hDisp->panels[hDisp->nPanels];
        hDisp->nPanels++;

        p->hPanel = 0; // no ODM adaptation
        p->Config = 0;
        p->builtin = hal->Hdmi;
    }

    /* go back over the panels, find the builtins, setup the device */
    for( i = 0; i < hDisp->nPanels; i++ )
    {
        NvDdkDispPanel *p = &hDisp->panels[i];
        if( !p->builtin )
        {
            continue;
        }

        p->nModes = 0;
        p->builtin->ListModes( &p->nModes, 0 );
        p->nModes = NV_MIN( NVDDK_DISP_MAX_MODES, p->nModes );
        p->builtin->ListModes( &p->nModes, p->modes );

        p->builtin->GetParameter( NvOdmDispParameter_Usage,
            (NvU32 *)&p->Usage );
        p->builtin->GetParameter( NvOdmDispParameter_Type,
            (NvU32 *)&p->Type );

        if( (p->Type == NvOdmDispDeviceType_HDMI && bHdmiPrimary) ||
            (p->Type == NvOdmDispDeviceType_CRT && bCrtPrimary) )
        {
            p->Usage = NvOdmDispDeviceUsage_Primary;
        }

        p->builtin->GetParameter(
            NvOdmDispParameter_MaxHorizontalResolution,
            (NvU32 *)&p->MaxHorizontalResolution );
        p->builtin->GetParameter(
            NvOdmDispParameter_MaxVerticalResolution,
            (NvU32 *)&p->MaxVerticalResolution );
        p->builtin->GetParameter( NvOdmDispParameter_BaseColorSize,
            (NvU32 *)&p->BaseColorSize );
        p->builtin->GetParameter( NvOdmDispParameter_DataAlignment,
            (NvU32 *)&p->DataAlignment );
        p->builtin->GetParameter( NvOdmDispParameter_PinMap,
            (NvU32 *)&p->PinMap );
        p->ColorCalibrationRed = 0x0;
        p->ColorCalibrationGreen = 0x0;
        p->ColorCalibrationBlue = 0x0;
        p->builtin->GetParameter( NvOdmDispParameter_PwmOutputPin,
           (NvU32 *)&p->PwmOutputPin );

        /* calculate mode frequencies */
        NvDdkDispPrivSetupModeFreqs( p );
    }

    #undef GET_ODM_ATTR
    #undef GET_SUPPORTED_ODM_ATTR

    /* assign displays to a panel */
    for( i = 0; i < hDisp->nPanels; i++ )
    {
        NvDdkDispPanel *panel = &hDisp->panels[i];

        /* find the display that matches the usage */
        for( j = 0; j < caps->NumDisplays; j++ )
        {
            disp = &hDisp->displays[j];
            if( NvDdkDispPrivCompareUsage( panel->Usage, disp->attr.Usage ) &&
                disp->panel == 0 )
            {
                disp->panel = panel;
                disp->attr.Type = NvDdkDispOdmToDdkDeviceType( panel->Type );
                disp->attr.MaxHorizontalResolution =
                    panel->MaxHorizontalResolution;
                disp->attr.MaxVerticalResolution =
                    panel->MaxVerticalResolution;
                disp->attr.DsiInstance =
                    panel->DsiInstance;

                if (panel->BacklightInitialIntensity)
                    disp->attr.InBacklightIntensity =
                        (NvU8)panel->BacklightInitialIntensity;

                switch( panel->BaseColorSize ) {
                case NvOdmDispBaseColorSize_111:
                    disp->attr.ColorPrecision = 3;
                    break;
                case NvOdmDispBaseColorSize_222:
                    disp->attr.ColorPrecision = 6;
                    break;
                case NvOdmDispBaseColorSize_333:
                    disp->attr.ColorPrecision = 9;
                    break;
                case NvOdmDispBaseColorSize_444:
                    disp->attr.ColorPrecision = 12;
                    break;
                case NvOdmDispBaseColorSize_555:
                    disp->attr.ColorPrecision = 15;
                    break;
                case NvOdmDispBaseColorSize_565:
                    disp->attr.ColorPrecision = 16;
                    break;
                case NvOdmDispBaseColorSize_666:
                    disp->attr.ColorPrecision = 18;
                    break;
                case NvOdmDispBaseColorSize_332:
                    disp->attr.ColorPrecision = 8;
                    break;
                case NvOdmDispBaseColorSize_888:
                    disp->attr.ColorPrecision = 24;
                    break;
                default:
                    break;
                }

                if (panel->ColorCalibrationRed)
                    disp->attr.ScaleRed = panel->ColorCalibrationRed;
                if (panel->ColorCalibrationGreen)
                    disp->attr.ScaleGreen = panel->ColorCalibrationGreen;
                if (panel->ColorCalibrationBlue)
                    disp->attr.ScaleBlue = panel->ColorCalibrationBlue;

                break;
            }
        }
    }

    /* prevent strangeness with the primary display if:
     * 1) the system has a bootloader with a splash screen
     * 2) the bootargs don't flag this as such
     * 3) the controller used for the splash screen is then used to drive
     *    an external display.
     *
     * this happens in stand-alone unit tests. shut down the primary panel.
     */
    disp = &hDisp->displays[0];
    if( disp->panel->hPanel )
    {
        NvOdmDispSetMode( disp->panel->hPanel, 0, 0 );
    }

    #undef NVDDK_DISP_DEBUG_PANEL_SETUP

    return NV_TRUE;
}

static void
NvDdkDispPrivAttachableControllers( NvDdkDispHandle hDisp )
{
    NvU32 i;
    NvDdkDispDisplay *disp;

    for( i = 0; i < NVDDK_DISP_MAX_DISPLAYS; i++ )
    {
        disp = &hDisp->displays[i];
        if( !disp->panel )
        {
            disp->AttachableControllers = 0;
            continue;
        }
        disp->AttachableControllers = 0xFF;
    }
}

static NvBool
NvDdkDispPrivInitState( NvDdkDispHandle hDisp )
{
    NvError err;
    NvU32 i, j;
    NvDdkDispWindow *win;
    NvDdkDispDisplay *disp;
    NvDdkDispController *cont;
    NvDdkDispCapabilities *caps;
    NvBool b;

    caps = hDisp->caps;

    NV_ASSERT( caps->NumControllers > 0 );
    NV_ASSERT( caps->NumWindows > 0 );
    NV_ASSERT( caps->NumDisplays > 0 );

    /* setup controllers */
    for( i = 0; i < NVDDK_DISP_MAX_CONTROLLERS; i++ )
    {
        cont = &hDisp->controllers[i];
        cont->hDisp = hDisp;
        cont->Index = i;
        cont->HwInitialized = NV_FALSE;
        cont->mode_flags = NvDdkDispModeFlag_None;
        cont->state = ControllerState_Stopped;
        cont->pre_suspend_state = ControllerState_Stopped;
        cont->AttachMask = (NvU8)(1UL << i);
        cont->window_enable = 0;

        err = NvOsMutexCreate(&cont->mutex);
        if( err != NvSuccess )
        {
            goto fail;
        }

        NvOsMemset( &cont->mode, 0, sizeof(NvOdmDispDeviceMode) );

        NvOsMemset(cont->AttachedDisplays, 0, sizeof(*cont->AttachedDisplays));
        cont->nAttached = 0;

        NvOsMemset( &cont->attr, 0, sizeof(ControllerAttr) );

        /* cursor memory, 64x64, 2 bpp, 1024 byte align */
        b = NvDdkDispPrivAllocMem( hDisp->hDevice, &cont->cursor.hMem1,
            (64 * 64 * 2) / 8, 1024 );
        b = NvDdkDispPrivAllocMem( hDisp->hDevice, &cont->cursor.hMem2,
            (64 * 64 * 2) / 8, 1024 );
        cont->cursor.current = NULL;
        cont->cursor.pinned = NULL;
        cont->cursor.fgColor = 0xFFFFFFFF;
        cont->cursor.bgColor = 0;
        if( !b )
        {
            goto fail;
        }

        NvDdkDispHalInit( caps, &cont->hal );

        /* get the maximum frequency */
        cont->MaxFrequency = NvRmPowerModuleGetMaxFrequency( hDisp->hDevice,
            NVRM_MODULE_ID( NvRmModuleID_Display, cont->Index ) );
        if (!cont->MaxFrequency)
            cont->MaxFrequency = 300000;

        /* create the timeout thread */
        cont->timeout_thread = NvOsAlloc( sizeof(NvDdkDispTimeoutThread) );
        if( !cont->timeout_thread )
        {
            goto fail;
        }

        NvOsMemset( cont->timeout_thread, 0, sizeof(NvDdkDispTimeoutThread) );
        cont->timeout_thread->hController = cont;
        cont->timeout_thread->bShutdown = NV_FALSE;
        cont->timeout_thread->hal = &cont->hal;

        err = NvOsSemaphoreCreate(&cont->timeout_thread->sem, 0);
        if( err != NvSuccess )
        {
            goto fail;
        }

        err = NvOsThreadCreate( NvDdkDispBacklightTimeout,
            cont->timeout_thread,
            &cont->timeout_thread->thread );
        if( err != NvSuccess )
        {
            cont->timeout_thread->thread = 0;
            goto fail;
        }

        cont->error_thread = 0;
        if( NVDDK_DISP_CAP_IS_SET(caps, NvDdkDispCapBit_Bug_UnderflowReset ) )
        {
            cont->error_thread = NvOsAlloc( sizeof(NvDdkDispErrorThread) );
            if( !cont->error_thread )
            {
                goto fail;
            }

            NvOsMemset( cont->error_thread, 0,
                sizeof(NvDdkDispErrorThread) );
            cont->error_thread->hController = cont;
            cont->error_thread->bShutdown = NV_FALSE;
            cont->error_thread->hal = &cont->hal;

            err = NvOsSemaphoreCreate(&cont->error_thread->sem, 0);
            if( err != NvSuccess )
            {
                goto fail;
            }

            err = NvOsThreadCreate( NvDdkDispErrorHandler,
                cont->error_thread, &cont->error_thread->thread );
            if( err != NvSuccess )
            {
                cont->error_thread->thread = 0;
                goto fail;
            }
        }

        /* setup windows */
        for( j = 0; j < NVDDK_DISP_MAX_WINDOWS; j++ )
        {
            win = &cont->windows[j];
            win->hDisp = hDisp;
            win->hController = cont;
            win->mask = (1UL << j);
            NvOsMemset( win->surface, 0, sizeof(win->surface) );
            win->nSurfaces = 0;
            win->bColorKeyDirty = NV_FALSE;

            NvOsMemset( &win->attr, 0, sizeof(WindowAttr) );

            win->attr.Number = j;
            win->attr.Depth = (NvU32)-1;
            win->attr.Mirror = NvDdkDispMirror_None;
            win->attr.BlendType = NvDdkDispBlendType_None;
            win->attr.AlphaOperation = NvDdkDispAlphaOperation_WeightedMean;
            win->attr.AlphaDirection = NvDdkDispAlphaBlendDirection_Default;
            win->attr.AlphaValue = 0xff;
            win->attr.ColorSpaceCoef.K = (-16 << 16) + 1;
            // ( 1.1644 * (float)(1 << 16 ) + 0.5 )
            win->attr.ColorSpaceCoef.C11 = 76310;
            win->attr.ColorSpaceCoef.C12 = 0;
            // ( 1.596 * (float)(1 << 16) + 0.5 )
            win->attr.ColorSpaceCoef.C13 = 104595;
            // ( -0.3918 * (float)(1 << 16) + 0.5 )
            win->attr.ColorSpaceCoef.C22 = -25676;
            // ( -0.8130 * (float)(1 << 16) + 0.5 )
            win->attr.ColorSpaceCoef.C23 = -53280;
            // ( 2.0172 * (float)(1 << 16) + 0.5 )
            win->attr.ColorSpaceCoef.C32 = 132199;
            win->attr.ColorSpaceCoef.C33 = 0;

            /* setup window usage */
            if( i == 0 )
            {
                win->attr.Usage = NvDdkDispWindowUsage_Graphic;
            }
            else
            {
                win->attr.Usage = NvDdkDispWindowUsage_Overlay;
            }
        }
    }

    /* setup displays */
    for( i = 0; i < NVDDK_DISP_MAX_DISPLAYS; i++ )
    {
        disp = &hDisp->displays[i];
        disp->hDisp = hDisp;
        disp->panel = 0;
        disp->CurrentController = 0;
        disp->AttachableControllers = 0;
        disp->power = NvOdmDispDevicePowerLevel_Off;
        err = NvOsMutexCreate(&disp->mutex);
        if( err != NvSuccess )
        {
            goto fail;
        }

        NvOsMemset( &disp->attr, 0, sizeof(DisplayAttr) );
        NvOsMemset( &disp->edid, 0, sizeof(disp->edid) );

        disp->attr.Backlight = NvDdkDispBacklightControl_On;
        disp->attr.InBacklightIntensity = 255;
        disp->attr.OutBacklightIntensity = 255;
        disp->attr.Brightness = 0;
        disp->attr.GammaRed = 0x00010000;
        disp->attr.GammaGreen = 0x00010000;
        disp->attr.GammaBlue = 0x00010000;
        disp->attr.ScaleRed = 0x00010000;
        disp->attr.ScaleGreen = 0x00010000;
        disp->attr.ScaleBlue = 0x00010000;
        disp->attr.Contrast = 0x00010000;

        disp->attr.AudioFrequency = NvDdkDispAudioFrequency_44100hz;

        // Seems the image lean to right 6 pixels, shift it to left back to
        // center image on composite TVOUT
        disp->attr.TV_HorizontalPosition = 0x80060000;
        disp->attr.TV_VerticalPosition = 0;
        disp->attr.TV_Overscan = 1024;
        disp->attr.TV_OverscanY = 16;
        disp->attr.TV_OverscanCb = 128;
        disp->attr.TV_OverscanCr = 128;
        disp->attr.TV_OutputFormat = NvDdkDispTvOutput_Composite;
        disp->attr.TV_Type = NvDdkDispTvType_Ntsc;
        disp->attr.TV_FilterMode = NvDdkDispTvFilterMode_LowPass1;
        disp->attr.TV_DAC_Amplitude = NvRmAnalogGetTvDacConfiguration(
            hDisp->hDevice, NvRmAnalogTvDacType_SDTV );
        if( !disp->attr.TV_DAC_Amplitude )
        {
            disp->attr.TV_DAC_Amplitude = 0xC5;
        }
        disp->attr.TV_ScreenFormat = NvDdkDispTvScreenFormat_Standard_4_3;
    }

    /* assign panels to displays, etc */
    b = NvDdkDispPrivSetupPanels( hDisp );
    if( !b )
    {
        goto fail;
    }

    NvDdkDispPrivAttachableControllers( hDisp );

    return NV_TRUE;

fail:
    NvDdkDispClose( hDisp, 0 );
    return NV_FALSE;
}

NvError
NvDdkDispListControllers(NvDdkDispHandle hDisp,
    NvU32 *pNum,
    NvDdkDispControllerHandle *phController)
{
    NV_ASSERT( hDisp );
    NV_ASSERT( pNum );

    if( !phController )
    {
        *pNum = hDisp->caps->NumControllers;
    }
    else
    {
        NvU32 i;
        NvU32 len;

        len = NV_MIN( *pNum, hDisp->caps->NumControllers );
        *pNum = len;

        for( i = 0; i < len; i++ )
        {
            phController[i] = &hDisp->controllers[i];
        }
    }

    return NvSuccess;
}

NvError
NvDdkDispListDisplays( NvDdkDispControllerHandle hController,
    NvU32 *pNum,
    NvDdkDispDisplayHandle *phDisplay )
{
    NvU32 count = 0;
    NvDdkDispDisplayHandle disp;
    NvU32 i;

    NV_ASSERT( hController );
    NV_ASSERT( pNum );

    if( !phDisplay )
    {
        *pNum = 0;
    }

    NV_DEBUG_CODE(
        if( *pNum )
        {
            NV_ASSERT( phDisplay );
        }
        else
        {
            NV_ASSERT( phDisplay == 0 );
        }
    );

    /* enumerate displays that can attach to this controller and that
     * have a panel.
     */
    for( i = 0; i < hController->hDisp->caps->NumDisplays; i++ )
    {
        disp = &hController->hDisp->displays[i];
        if( ( disp->AttachableControllers & hController->AttachMask ) &&
            ( disp->panel ) )
        {
            if( *pNum && count < *pNum )
            {
                phDisplay[count] = disp;
            }

            count++;
        }
    }

    if( !(*pNum ) || (*pNum) > count )
    {
        *pNum = count;
    }

    return NvSuccess;
}

NvError
NvDdkDispGetDisplayByUsage(NvDdkDispControllerHandle hController,
    NvDdkDispDisplayUsageType usage,
    NvDdkDispDisplayHandle *phDisplay)
{
    NvDdkDispDisplayHandle disp = 0;
    NvU32 i;
    NvBool found = NV_FALSE;

    NV_ASSERT( hController );
    NV_ASSERT( phDisplay );

    /* find a display that can attach to the controller and matches the
     * given usage and has panel.
     */
    for( i = 0; i < hController->hDisp->caps->NumDisplays; i++ )
    {
        disp = &hController->hDisp->displays[i];

        if( ( disp->attr.Usage == usage ) &&
            ( disp->AttachableControllers & hController->AttachMask ) &&
            ( disp->panel ) )
        {
            found = NV_TRUE;
            break;
        }
    }

    if( !found )
    {
        return NvError_DispNotFound;
    }

    *phDisplay = disp;
    return NvSuccess;
}

NvError
NvDdkDispGetDisplayByGuid(NvDdkDispControllerHandle hController,
    NvU64 guid,
    NvDdkDispDisplayHandle *phDisplay)
{
    NvDdkDispDisplayHandle disp = 0;
    NvOdmDispDeviceHandle hOdmDevice;
    NvBool found = NV_FALSE;
    NvU32 i;

    NV_ASSERT( hController );
    NV_ASSERT( phDisplay );

    if( !guid )
    {
        NvOdmDispGetDefaultGuid( &guid );
    }

    /* look for the guid in the built-in displays */
    for( i = 0; i < hController->hDisp->caps->NumDisplays; i++ )
    {
        disp = &hController->hDisp->displays[i];
        if( disp->panel && !disp->panel->hPanel )
        {
            if( disp->panel->builtin->GetGuid() == guid )
            {
                *phDisplay = disp;
                return NvSuccess;
            }
        }
    }

    found = NvOdmDispGetDeviceByGuid( guid, &hOdmDevice );
    if( !found )
    {
        return NvError_DispNotFound;
    }

    /* find the display that matches */
    found = NV_FALSE;
    for( i = 0; i < hController->hDisp->caps->NumDisplays; i++ )
    {
        disp = &hController->hDisp->displays[i];

        if( ( disp->AttachableControllers & hController->AttachMask ) &&
            ( disp->panel ) &&
            ( disp->panel->hPanel == hOdmDevice ) )
        {
            found = NV_TRUE;
            break;
        }
    }

    if( !found )
    {
        *phDisplay = 0;
        return NvError_DispNotFound;
    }

    *phDisplay = disp;
    return NvSuccess;
}

NvError
NvDdkDispListWindows(NvDdkDispControllerHandle hController,
    NvU32 *pNum,
    NvDdkDispWindowHandle *phWindow)
{
    NV_ASSERT( hController );
    NV_ASSERT( pNum );

    if( !phWindow )
    {
        *pNum = 0;
    }

    if( !(*pNum ) )
    {
        NV_ASSERT( phWindow == 0 );
        *pNum = hController->hDisp->caps->NumWindows;
    }
    else
    {
        NvU32 i;
        NvU32 len;

        len = NV_MIN( *pNum, hController->hDisp->caps->NumWindows );
        *pNum = len;

        for( i = 0; i < len; i++ )
        {
            phWindow[i] = &hController->windows[i];
        }
    }

    return NvSuccess;
}

NvError
NvDdkDispFlush( NvDdkDispControllerHandle hController,
    NvU32 flags )
{
    NvError e;
    NvU32 i;
    NvU32 nWin;
    NvDdkDispWindowHandle hWindow;
    NvBool b;
    NvBool bDirty = NV_FALSE;

    NV_ASSERT( hController );
    NV_ASSERT( flags == 0 );

    NvOsMutexLock( hController->mutex );

    if( hController->state != ControllerState_Active )
    {
        e = NvSuccess;
        goto clean;
    }

    /* only window objects support the do-not-commit flag yet, so just check
     * those.
     */
    nWin = hController->hDisp->caps->NumWindows;
    for( i = 0; i < nWin; i++ )
    {
        hWindow = &hController->windows[i];

        if( hWindow->bDirty )
        {
            bDirty = NV_TRUE;
            break;
        }
    }

    if( bDirty )
    {
        b = NvDdkDispPrivModeCommit( hController );
        if( !b )
        {
            goto fail;
        }

        if( hController->mode.flags & NVDDK_DISP_MODE_FLAG_USE_TEARING_EFFECT )
        {
            NvDdkDispHal *hal = &hController->hal;
            hal->HwTriggerFrame( hController->Index );
        }
    }

    e = NvSuccess;
    goto clean;

fail:
    // FIXME: error code
    e = NvError_BadParameter;

clean:
    NvOsMutexUnlock( hController->mutex );
    return e;
}

void
NvDdkDispCacheClear( NvDdkDispControllerHandle hController,
    NvU32 flags )
{
    NvDdkDispHal *hal;

    NV_ASSERT( hController );
    NV_ASSERT( flags == NVDDK_DISP_CACHE_CLEAR_ALL );

    NvOsMutexLock( hController->mutex );

    hal = &hController->hal;
    hal->HwCacheClear( hController->Index, flags );

    NvOsMutexUnlock( hController->mutex );
}

static void
NvDdkDispPrivSanitizeMode( NvDdkDispMode *m )
{
    // FIXME: why is this necessary?
    if( !m->refresh )
    {
        m->refresh = (60 << 16);
    }

    // FIXME: bPartialMode is depricated
    if( m->bPartialMode )
    {
        m->flags |= NVODM_DISP_MODE_FLAG_PARTIAL;
    }
}

static NvBool
NvDdkDispPrivEdid( NvDdkDispDisplayHandle hDisplay )
{
    NvError err;
    NvDdkDispEdid *edid;

    /* only hdmi and crts support edids */
    if( hDisplay->attr.Type == NvDdkDispDisplayType_CRT ||
        hDisplay->attr.Type == NvDdkDispDisplayType_HDMI )
    {
        NvBool b = NV_TRUE;
        NvDdkDispPanel *p = hDisplay->panel;
        edid = &hDisplay->edid;

        NvOsMemset( edid, 0, sizeof(NvDdkDispEdid) );

        /* both crt and hdmi are built-in types these days */
        NV_ASSERT( hDisplay->panel->builtin );

        /* read edid */
        err = NvDdkDispEdidRead( hDisplay->hDisp->hDevice, hDisplay, edid );
        if( err != NvSuccess )
        {
            NvOsMemset( edid, 0, sizeof(NvDdkDispEdid) );

            if (NVDDK_DISP_CAP_IS_SET(hDisplay->hDisp->caps,
                NvDdkDispCapBit_ForceDefaultExternalDisplayMode))
            {
                hDisplay->panel->builtin->SetEdid( 0,
                    NVDDK_DISP_SET_EDID_FLAG_USE_DEFAULT );
            }
            else
            {
                // use vesa modes on failure
                hDisplay->panel->builtin->SetEdid( 0,
                    NVDDK_DISP_SET_EDID_FLAG_USE_VESA );
            }
            b = NV_FALSE;
        }
        else
        {
            if (NVDDK_DISP_CAP_IS_SET(hDisplay->hDisp->caps,
                NvDdkDispCapBit_ForceDefaultExternalDisplayMode))
            {
                hDisplay->panel->builtin->SetEdid( &hDisplay->edid,
                    NVDDK_DISP_SET_EDID_FLAG_USE_DEFAULT );
            }
            else
            {
                hDisplay->panel->builtin->SetEdid( &hDisplay->edid, 0 );
            }
        }

        /* re-list the display modes */
        p->nModes = 0;
        p->builtin->ListModes( &p->nModes, 0 );
        p->nModes = NV_MIN( NVDDK_DISP_MAX_MODES, p->nModes );
        p->builtin->ListModes( &p->nModes, p->modes );

        return b;
    }

    return NV_FALSE;
}

static NvS32 NvDdkDispPrivGetMaxFrequency( NvDdkDispDisplayHandle hDisplay )
{
    NvS32 max_freq = 0, tmp_freq = 0;

    /* if this display is attached to a controller, then verify that the chip
     * can support the requested frequency
     */
    if( hDisplay->CurrentController )
    {
        max_freq = hDisplay->CurrentController->MaxFrequency;
    }

    /* downstream output resources may have other clock frequency
     * limitations.
     */
    switch( hDisplay->attr.Type ) {
    case NvDdkDispDisplayType_HDMI:
        tmp_freq = NvRmPowerModuleGetMaxFrequency( hDisplay->hDisp->hDevice,
            NVRM_MODULE_ID( NvRmModuleID_Hdmi, 0 ) );
        break;
    case NvDdkDispDisplayType_DSI:
        tmp_freq = NvRmPowerModuleGetMaxFrequency( hDisplay->hDisp->hDevice,
            NVRM_MODULE_ID( NvRmModuleID_Dsi, 0 ) );
        break;
    default:
        break;
    }

    if( tmp_freq && max_freq )
    {
        max_freq = NV_MIN( tmp_freq, max_freq );
    }

    return max_freq;
}

NvError
NvDdkDispListDisplayModes(
    NvDdkDispDisplayHandle hDisplay,
    NvU32 *pNum,
    NvDdkDispMode *pMode,
    NvU32 flags)
{
    NvError err;
    NvU32 count;
    NvU32 i;
    NvDdkDispPanel *panel;
    NvOdmDispDeviceMode *modes;
    NvS32 max_freq = 0;
    NvBool bSkipEdid = NV_FALSE;
    NvBool bUseBuiltin = NV_FALSE;
    NvBool bEdidErrorNotify = NV_FALSE;
    NvBool bEdidFailed = NV_FALSE;

    if( flags & NVDDK_DISP_LISTMODES_SKIP_EDID )
    {
        bSkipEdid = NV_TRUE;
    }
    flags &= ~NVDDK_DISP_LISTMODES_SKIP_EDID;

    if( flags & NVDDK_DISP_LISTMODES_RESERVED )
    {
        bUseBuiltin = NV_TRUE;
    }
    flags &= ~NVDDK_DISP_LISTMODES_RESERVED;

    if( flags & NVDDK_DISP_LISTMODES_EDID_ERROR )
    {
        bEdidErrorNotify = NV_TRUE;
    }
    flags &= ~NVDDK_DISP_LISTMODES_EDID_ERROR;

    NV_ASSERT( hDisplay );
    NV_ASSERT( pNum );
    NV_ASSERT( ( flags == NVDDK_DISP_LISTMODES_ALL ) ||
               ( flags == NVDDK_DISP_LISTMODES_FULL ) ||
               ( flags == NVDDK_DISP_LISTMODES_PARTIAL ) );
    NV_ASSERT( hDisplay->panel );

    NV_DEBUG_CODE(
        if( *pNum )
        {
            NV_ASSERT( pMode );
        }
    );

    count = 0;
    panel = hDisplay->panel;
    modes = panel->modes;

    /* check for plug-n-play panels */
    if( !bSkipEdid )
    {
        NvBool b;

        b = NvDdkDispPrivEdid( hDisplay );
        if( !b )
        {
            /* default vesa modes will be filled in, need to continue here
             * to export the modes, but return a special failure case.
             */
            bEdidFailed = NV_TRUE;
        }
    }
    else
    {
        if( (hDisplay->attr.Type == NvDdkDispDisplayType_CRT) ||
            (hDisplay->attr.Type == NvDdkDispDisplayType_HDMI) )
        {
            NvU32 f = 0;

            NV_ASSERT( hDisplay->panel->builtin );

            if (NVDDK_DISP_CAP_IS_SET(hDisplay->hDisp->caps,
                NvDdkDispCapBit_ForceDefaultExternalDisplayMode))
            {
                f = NVDDK_DISP_SET_EDID_FLAG_USE_DEFAULT;
            }
            else if( bUseBuiltin )
            {
                f = NVDDK_DISP_SET_EDID_FLAG_USE_BUILTIN;
            }
            else
            {
                f = NVDDK_DISP_SET_EDID_FLAG_USE_ALL;
            }

            hDisplay->panel->builtin->SetEdid( 0, f );

            /* re-list the display modes */
            panel->nModes = 0;
            panel->builtin->ListModes(&panel->nModes, 0);
            panel->nModes = NV_MIN( NVDDK_DISP_MAX_MODES, panel->nModes );
            panel->builtin->ListModes(&panel->nModes, panel->modes);
        }
    }

    max_freq = NvDdkDispPrivGetMaxFrequency( hDisplay );

    for( i = 0; i < panel->nModes; i++ )
    {
        if( *pNum && count >= *pNum )
        {
            err = NvSuccess;
            goto clean;
        }

        /* skip a mode that's not supported */
        if( max_freq && max_freq < modes[i].frequency )
        {
            continue;
        }

        if( ( flags == NVDDK_DISP_LISTMODES_ALL ) ||
            ( (modes[i].flags & NVODM_DISP_MODE_FLAG_PARTIAL) &&
                ( flags & NVDDK_DISP_LISTMODES_PARTIAL )) ||
            ( !(modes[i].flags & NVODM_DISP_MODE_FLAG_PARTIAL) &&
                ( flags & NVDDK_DISP_LISTMODES_FULL )))
        {
            /* copy out the mode, if required */
            if( *pNum )
            {
                NvDdkDispPrivSanitizeMode( &modes[i] );
                *pMode = modes[i];
                pMode++;
            }

            count++;
        }
    }

    if( *pNum == 0 || *pNum > count )
    {
        *pNum = count;
    }

    if( bEdidFailed && bEdidErrorNotify )
    {
        return NvError_DispEdidFailure;
    }

    err = NvSuccess;

clean:
    return err;
}

// XXX Can this function be removed?  Is this really needed?
static void
NvDdkDispPrivSetTristateLevel(NvDdkDispDisplayHandle hDisplay,
    NvBool EnableTristate)
{
}

static void
NvDdkDispPrivSetPowerLevel( NvDdkDispDisplayHandle hDisplay,
    NvOdmDispDevicePowerLevel level )
{
    NvDdkDispPanel *panel;
    panel = hDisplay->panel;

    if( hDisplay->power == level )
    {
        return;
    }

    //  Prior to the display transitioning from Off to any other state,
    //  Un-tristate the relevant display pins
    if( hDisplay->power == NvOdmDispDevicePowerLevel_Off &&
        hDisplay->CurrentController )
    {
        NvDdkDispPrivSetTristateLevel(hDisplay, NV_FALSE);
    }

    if( panel->hPanel )
    {
        NvOdmDispSetPowerLevel( panel->hPanel, level );
    }
    else
    {
        NV_ASSERT( panel->builtin );
        panel->builtin->SetPowerLevel( level );
    }

    //  After transitioning the display from any state to Off, put the
    //  pins back in tristate
    if( level == NvOdmDispDevicePowerLevel_Off &&
        hDisplay->CurrentController )
    {
        NvDdkDispPrivSetTristateLevel(hDisplay, NV_TRUE);
    }

    hDisplay->power = level;
}

static void
NvDdkDispPrivCommitBacklight( NvDdkDispControllerHandle hController )
{
    NvDdkDispHal *hal;
    NvDdkDispDisplayHandle hDisplay;
    NvDdkDispPanel *panel;
    NvU32 i;
    NvU8 intensity = 0;

    hal = &hController->hal;

    for( i = 0; i < hController->nAttached; i++ )
    {
        hDisplay = hController->AttachedDisplays[i];
        panel = hDisplay->panel;
        if( hDisplay->attr.Backlight == NvDdkDispBacklightControl_On ||
            hDisplay->attr.Backlight == NvDdkDispBacklightControl_OnStrobe   )
        {
            intensity = hDisplay->attr.InBacklightIntensity;
        }

        hal->HwSetBacklight( hController->Index, intensity );
        hDisplay->attr.OutBacklightIntensity = intensity;
        if( panel->hPanel )
        {
            NvOdmDispSetBacklight( panel->hPanel, intensity );
        }
    }
}

static NvBool
NvDdkDispPrivModeCommit( NvDdkDispControllerHandle hController )
{
    NvDdkDispHandle hDisp;
    NvDdkDispCapabilities *caps;
    NvDdkDispWindowHandle hWindow;
    NvDdkDispHal *hal;
    NvU32 i, n;
    NvBool bEnable;
    NvBool bReopen = NV_FALSE;
    NvBool b;

    /* note for transition from bootloader: the reopen flag must be passed
     * into the controller HAL and the ODM panel adapation before setting
     * the power state to ON.
     *
     * NvDdkDispPrivUpdatePanels takes care of the panels and the reopen flag.
     */

    hal = &hController->hal;
    caps = hController->hDisp->caps;
    hDisp = hController->hDisp;

    if( hController->state != ControllerState_Active )
    {
        /* initialize hardware, if necessary */
        if( !hController->HwInitialized )
        {
            NvRmDeviceHandle hRm;

            hRm = hController->hDisp->hDevice;
            NV_ASSERT( hRm );

            b = hal->HwOpen( hRm, hController->Index, caps, bReopen );
            if( !b )
            {
                /* clear the mode */
                NvOsMemset( &hController->mode, 0, sizeof(hController->mode) );
                goto fail;
            }

            hController->HwInitialized = NV_TRUE;
        }

        /* commit all controller attributes to hardware, including
         * hw windows and all attached displays.
         */
        b = NvDdkDispPrivCommitAllAttributes( hController );
        if( !b )
        {
            goto fail;
        }
    }

    /* panel mode-set will need to know if this is a reopen. */
    hDisp->bReopen = bReopen;

    hal->HwBegin( hController->Index );

    /* may need to flush window attributes */
    n = hController->hDisp->caps->NumWindows;
    for( i = 0; i < n; i++ )
    {
        hWindow = &hController->windows[i];
        if( hWindow->bDirty )
        {
            NvDdkDispPrivCommitWindow( hWindow );
        }
    }

    /* notify window hardware */
    NvDdkDispPrivUpdateSurface( hController,
        &hController->windows[0],
        &hController->windows[caps->NumWindows-1],
        &bEnable );

    /* let panels get a chance to do any pre-pixel clock init */
    if( hController->state != ControllerState_Active )
    {
        NvDdkDispPrivUpdatePanels( hController,
            NvOdmDispDevicePowerLevel_PrePower );
    }

    /* set the mode on the controller */
    if( hController->state != ControllerState_Active ||
        hController->bModeDirty )
    {
        hal->HwSetMode( hController->Index, &hController->mode,
            hController->mode_flags );
        /* don't clear the dirty flag until after the panel update */
    }

    /* commit to hardware -- pixel clock may be enabled here */
    b = hal->HwEnd( hController->Index, NVDDK_DISP_WAIT );
    if( !b )
    {
        goto fail;
    }

    /* may need to trigger a frame update -- doing this here here
     * to let the panel's framebuffer get filled with good pixels before
     * enabling the backlight.
     */
    if( bEnable &&
       (hController->mode.flags & NVDDK_DISP_MODE_FLAG_USE_TEARING_EFFECT) )
    {
        hal->HwTriggerFrame( hController->Index );
    }

    /* this is necessary to have a clean transition from no pixel clock to
     * pixel clock.
     */
    hal->HwBegin( hController->Index );

    if( bEnable )
    {
        /* enable all attached panels, pixel clock is on */
        NvDdkDispPrivUpdatePanels( hController, NvOdmDispDevicePowerLevel_On );
    }

    /* enable backlight */
    if( bEnable && hController->state != ControllerState_Active )
    {
        NvDdkDispPrivCommitBacklight( hController );
    }

    /* commit to hardware */
    b = hal->HwEnd( hController->Index, NVDDK_DISP_WAIT );
    if( !b )
    {
        goto fail;
    }

    hController->state = ControllerState_Active;
    hController->bModeDirty = NV_FALSE;

    return NV_TRUE;

fail:
    return NV_FALSE;
}

static NvBool
NvDdkDispPrivPowerHint( NvDdkDispHandle hDisp )
{
    /* instantaneous bandwidth may cause problems with underflow. the RM
     * will turn down clocks given average usage, not instantenous. use the
     * busy hint api to fix things. newer chips may not have this issue, so
     * use a cap bit to avoid wasting bandwidth with better hardware.
     */

    NvError e;
    NvDdkDispCapabilities *caps;
    NvRmFreqKHz freq;
    NvU32 i, j;
    NvRmDfsBusyHint hint[2];

    caps = hDisp->caps;
    if( !NVDDK_DISP_CAP_IS_SET(caps, NvDdkDispCapBit_Bug_BandwidthPowerHint) )
    {
        return NV_TRUE;
    }

    if( !hDisp->PowerClientId )
    {
        NV_CHECK_ERROR_CLEANUP(
            NvRmPowerRegister( hDisp->hDevice, 0, &hDisp->PowerClientId )
        );
    }

    NvOsMemset( hint, 0, sizeof(hint) );

    /* set EMC to a frequency floor of:
     *
     * A = Sum over controllers: (controller frequency * Bpp for each window)
     * Floor = A / EMC atom size
     */
    freq = 0;
    for( i = 0; i < caps->NumControllers; i++ )
    {
        NvDdkDispControllerHandle hController;
        NvDdkDispWindowHandle hWindow;
        NvU32 wBpp = 0;

        hController = &hDisp->controllers[i];

        if( hController->state != ControllerState_Active )
        {
            continue;
        }

        for( j = 0; j < caps->NumWindows; j++ )
        {
            hWindow = &hController->windows[j];

            if( !(hController->window_enable & hWindow->mask) )
            {
                continue;
            }

            wBpp += (NV_COLOR_GET_BPP(hWindow->surface[0].ColorFormat) / 8);
        }

        freq += (hController->mode.frequency * wBpp);
    }

    freq = freq / 8; // EMC is 128bits per 2 clocks, which is 8 bytes per clock
    freq <<= 1;      // Need to have 50 % margin to be safe.

    hint[0].ClockId = NvRmDfsClockId_Emc;
    // rest of hint[0] shoud be zero
    hint[1].ClockId = NvRmDfsClockId_Emc;
    hint[1].BoostDurationMs = NV_WAIT_INFINITE;
    hint[1].BoostKHz = freq;

    e = NvRmPowerBusyHintMulti( hDisp->hDevice,
            hDisp->PowerClientId, hint, NV_ARRAY_SIZE(hint),
            NvRmDfsBusyHintSyncMode_Async );
    if( e != NvSuccess )
    {
        NV_ASSERT( !"power hint failed" );
        goto fail;
    }

    return NV_TRUE;

fail:
    return NV_FALSE;
}

static NvBool
NvDdkDispPrivIsModeNull( NvOdmDispDeviceMode *m )
{
    if( m->bpp == 0 &&
        m->width == 0 &&
        m->height == 0 &&
        m->flags == 0 )
    {
        return NV_TRUE;
    }

    return NV_FALSE;
}

/* attaches a display to a controller -- assumes hDisplay is locked */
static NvError
NvDdkDispPrivAttachDisplay( NvDdkDispControllerHandle hController,
    NvDdkDispDisplayHandle hDisplay, NvU32 flags )
{
    NvError err;
    NvU32 i;
    NvDdkDispCapabilities *caps;

    NV_ASSERT( hController );
    NV_ASSERT( hDisplay );

    caps = hController->hDisp->caps;
    if( ( hDisplay->attr.Type == NvDdkDispDisplayType_TV &&
          !NVDDK_DISP_CAP_IS_SET( caps, NvDdkDispCapBit_Tv)) ||
        ( hDisplay->attr.Type == NvDdkDispDisplayType_CRT &&
          !NVDDK_DISP_CAP_IS_SET( caps, NvDdkDispCapBit_Crt)) ||
        ( hDisplay->attr.Type == NvDdkDispDisplayType_HDMI &&
          !NVDDK_DISP_CAP_IS_SET( caps, NvDdkDispCapBit_Hdmi )) )
    {
        return NvError_DispTypeNotSupported;
    }

    NvOsMutexLock( hController->mutex );

    if( hDisplay->CurrentController )
    {
        err = NvError_DispAlreadyAttached;
        goto clean;
    }
    if( !( hDisplay->AttachableControllers & hController->AttachMask ) )
    {
        err = NvError_DispAttachDissallowed;
        goto clean;
    }

    /* only allowed to attach a display when either, the controller is
     * not active, or the REATTACH flag is set.
     */
    if( hController->state != ControllerState_Stopped &&
        !(flags & NVDDK_DISP_ATTACHDISPLAY_REATTACH) )
    {
        err = NvError_DispAttachDissallowed;
        goto clean;
    }

    for( i = 0; i < NVDDK_DISP_MAX_ATTACHED_DISPLAYS; i++ )
    {
        if( hController->AttachedDisplays[i] == 0 )
        {
            hController->AttachedDisplays[i] = hDisplay;
            break;
        }
    }
    if( i == NVDDK_DISP_MAX_ATTACHED_DISPLAYS )
    {
        err = NvError_DispTooManyDisplays;
        goto clean;
    }

    // FIXME: there's no best mode detection yet, only allow one display
    // at a time.
    if( hController->nAttached )
    {
        err = NvError_DispAttachDissallowed;
        goto clean;
    }

    switch( hDisplay->attr.Type ) {
    case NvDdkDispDisplayType_TFTLCD:
    {
        NvOdmDispTftConfig *cfg;
        cfg = (NvOdmDispTftConfig *)hDisplay->panel->Config;

        if( cfg->flags & NvOdmDispTftFlag_DataEnableMode )
        {
            hController->mode_flags |= NvDdkDispModeFlag_DataEnableMode;
        }
        break;
    }
    case NvDdkDispDisplayType_CLILCD:
    {
        NvOdmDispCliConfig *cfg;
        cfg = (NvOdmDispCliConfig *)hDisplay->panel->Config;

        if( cfg->flags & NvOdmDispCliFlag_DisableClockInBlank )
        {
            hController->mode_flags |= NvDdkDispModeFlag_DisableClockInBlank;
        }
        break;
    }
    case NvDdkDispDisplayType_TV:
        hController->mode_flags |= NvDdkDispModeFlag_TV;
        break;
    case NvDdkDispDisplayType_CRT:
        hController->mode_flags |= NvDdkDispModeFlag_CRT;
        break;
    case NvDdkDispDisplayType_HDMI:
        if( hController->hDisp->bDsiPresent )
        {
            break;
        }
        // fall through - use the mipi flag since there's no DSI panel
    case NvDdkDispDisplayType_DSI:
        /* need the low-jitter pll */
        hController->mode_flags |= NvDdkDispModeFlag_MIPI;
    default:
        break;
    }

    hDisplay->CurrentController = hController;
    hController->nAttached++;

    if( hController->state == ControllerState_Stopped &&
        (flags & NVDDK_DISP_ATTACHDISPLAY_REATTACH) )
    {
        /* re-start the controller */
        if( !NvDdkDispPrivModeCommit( hController ) )
        {
            /* close-enough error code */
            err = NvError_DispAttachDissallowed;
            goto clean;
        }
    }

    err = NvSuccess;

clean:
    NvOsMutexUnlock( hController->mutex );
    return err;
}

/** detaches a display from a controller -- assuming hDisplay is locked */
static NvError
NvDdkDispPrivDetachDisplay( NvDdkDispControllerHandle hController,
    NvDdkDispDisplayHandle hDisplay, NvU32 flags )
{
    NvError err;
    NvU32 i;
    NvDdkDispHal *hal;
    NvBool b;

    (void)b;
    NV_ASSERT( hController );
    NV_ASSERT( hDisplay );

    NvOsMutexLock( hController->mutex );

    if( !hDisplay->CurrentController )
    {
        err = NvError_DispNotAttached;
        goto clean;
    }
    if( hController->nAttached == 0 )
    {
        err = NvError_DispNoDisplaysAttached;
        goto clean;
    }

    hal = &hController->hal;

    for( i = 0; i < NVDDK_DISP_MAX_ATTACHED_DISPLAYS; i++ )
    {
        if( hDisplay->CurrentController->AttachedDisplays[i] == hDisplay )
        {
            hDisplay->CurrentController->AttachedDisplays[i] = 0;
            break;
        }
    }
    if( i == NVDDK_DISP_MAX_ATTACHED_DISPLAYS )
    {
        /* didn't find the display to detach */
        err = NvError_BadParameter;
        goto clean;
    }

    /* only clear flags if this is the last attached display (all attached
     * displays must share the same flags)
     */
    if( hController->nAttached == 1 )
    {
        switch( hDisplay->attr.Type ) {
        case NvDdkDispDisplayType_TFTLCD:
            hController->mode_flags &= ~NvDdkDispModeFlag_DisableClockInBlank;
            break;
        case NvDdkDispDisplayType_TV:
            hController->mode_flags &= ~NvDdkDispModeFlag_TV;
            break;
        case NvDdkDispDisplayType_CRT:
            hController->mode_flags &= ~NvDdkDispModeFlag_CRT;
            break;
        case NvDdkDispDisplayType_HDMI: // fall through
        case NvDdkDispDisplayType_DSI:
            hController->mode_flags &= ~NvDdkDispModeFlag_MIPI;
        default:
            break;
        }
    }

    hDisplay->CurrentController->nAttached--;

    if( hDisplay->CurrentController->state == ControllerState_Active )
    {
        NvDdkDispPanel *panel;
        panel = hDisplay->panel;

        /* disable the display device */
        if( flags & NVDDK_DISP_ATTACHDISPLAY_LEAVEON )
        {
            /* notify the panel that the display controller is detaching */
            NvDdkDispPrivSetPowerLevel( hDisplay,
                NvOdmDispDevicePowerLevel_SelfRefresh );
        }
        else if( !(flags & NVDDK_DISP_ATTACHDISPLAY_LEAVEON) &&
            hDisplay->power == NvOdmDispDevicePowerLevel_On )
        {
            if( panel->hPanel )
            {
                NvOdmDispSetMode( panel->hPanel, 0, 0 );
            }

            NvDdkDispPrivSetPowerLevel( hDisplay,
                NvOdmDispDevicePowerLevel_Off );

            hal->HwBegin( hController->Index );

            switch( hDisplay->panel->Type ) {
            case NvDdkDispDisplayType_TFTLCD:
                break;
            case NvDdkDispDisplayType_TV:
            case NvDdkDispDisplayType_CRT:
            case NvDdkDispDisplayType_HDMI:
                panel->builtin->Enable( hController->Index, NV_FALSE );
                break;
            case NvDdkDispDisplayType_DSI:
                hal->HwDsiEnable( hController->Index, panel->hPanel,
                    hDisplay->panel->DsiInstance, NV_FALSE );
                break;
            default:
                break;
            }

            b = hal->HwEnd( hController->Index, 0 );
            NV_ASSERT( b );
        }

        if( !hDisplay->CurrentController->nAttached )
        {
            /* turn off the display controller */

            hController->state = ControllerState_Stopped;
            hController->HwInitialized = NV_FALSE;

            /* commit to hardware */
            hal->HwClose( hController->Index );
        }
    }

    hDisplay->CurrentController = 0;

    /* zero out the edid */
    NvOsMemset( &hDisplay->edid, 0, sizeof(hDisplay->edid) );

    err = NvSuccess;

clean:
    NvOsMutexUnlock( hController->mutex );
    return err;
}

NvError
NvDdkDispAttachDisplay(
    NvDdkDispControllerHandle hController,
    NvDdkDispDisplayHandle hDisplay,
    NvU32 flags)
{
    NvError err;

    NV_ASSERT( hDisplay );
    NV_ASSERT( flags == 0 ||
        !( (flags & ~NVDDK_DISP_ATTACHDISPLAY_LEAVEON
                  & ~NVDDK_DISP_ATTACHDISPLAY_REATTACH )));

    NvOsMutexLock( hDisplay->mutex );

    if( hController )
    {
        /* attaching the display to a controller */
        err = NvDdkDispPrivAttachDisplay( hController, hDisplay, flags );
        if( err != NvSuccess )
        {
            goto clean;
        }
    }
    else
    {
        /* detaching the display from the controller */
        hController = hDisplay->CurrentController;

        if( hController )
        {
           err = NvDdkDispPrivDetachDisplay( hController, hDisplay, flags );
           if( err != NvSuccess )
           {
               goto clean;
           }
        }
    }

    err = NvSuccess;

clean:
    NvOsMutexUnlock( hDisplay->mutex );
    return err;
}

NvError
NvDdkDispIsDisplayAttached(
    NvDdkDispDisplayHandle hDisplay,
    NvDdkDispControllerHandle *phController,
    NvU32 flags)
{
    NV_ASSERT( hDisplay );
    NV_ASSERT( flags == 0 );

    NvOsMutexLock( hDisplay->mutex );
    *phController = hDisplay->CurrentController;
    NvOsMutexUnlock( hDisplay->mutex );

    return NvSuccess;
}

static NvBool NvDdkDispPrivUpdate( NvDdkDispDisplayHandle hDisplay,
    NvDdkDispWindowHandle hWindow, NvRmSurface *surf, NvU32 count,
    NvPoint *pSrc, NvRect *pUpdate, NvRmStream *stream, NvRmFence *fence,
    NvU32 flags )
{
    NvError e;
    NvDdkDispControllerHandle hController;

    NV_ASSERT( hDisplay->panel );

    hController = hDisplay->CurrentController;

    if( !(hController->mode.flags & NVDDK_DISP_MODE_FLAG_USE_TEARING_EFFECT) )
    {
        /* just set the surface as a fallback -- this function shouldn't have
         * been called, but failing the call is not very friendly.
         */
        e = NvDdkDispSetWindowSurface( hWindow, surf, count, NVDDK_DISP_WAIT );
        if( e != NvSuccess )
        {
            return NV_FALSE;
        }

        return NV_TRUE;
    }

    if( hDisplay->attr.Type == NvDdkDispDisplayType_DSI )
    {
        const NvOdmDispDsiConfig *info;
        /* for dc-driven command mode, just do a surface set rather than host
         * command mode since the surface bits will get transmitted via the
         * hardware, etc.
         */
        info = (NvOdmDispDsiConfig *)hDisplay->panel->Config;
        if( info->VideoMode != NvOdmDsiVideoModeType_DcDrivenCommandMode )
        {
            /* no support for host-only command mode */
            return NV_FALSE;
        }
    }

    // FIXME: ignoring partial dest rect updates

    /* always go through NvDdkDispFlipWindowSurface to get the synchronization
     * correct with the streams/fences. It's tempting to just poke the trigger,
     * but there are race conditions with clients that are still rendering to
     * the surface.
     */
    e = NvDdkDispFlipWindowSurface( hWindow, surf, count, stream, fence,
            flags );
    if( e != NvSuccess )
    {
        goto fail;
    }

    return NV_TRUE;

fail:
    return NV_FALSE;
}

NvError
NvDdkDispDisplayUpdate(
    NvDdkDispDisplayHandle hDisplay,
    NvDdkDispWindowHandle hWindow,
    NvRmSurface *surf,
    NvU32 count,
    NvPoint *pSrc,
    NvRect *pUpdate,
    NvRmStream *stream,
    NvRmFence *fence,
    NvU32 flags )
{
    NvError e;
    NvBool b;

    NV_ASSERT( hDisplay );
    NV_ASSERT( surf );
    NV_ASSERT( count );

    if( NvDdkDispPrivIsSurfaceNull(surf) )
    {
        return NvError_BadParameter;
    }

    NvOsMutexLock( hDisplay->mutex );

    switch( hDisplay->attr.Type ) {
    case NvDdkDispDisplayType_DSI:
    case NvDdkDispDisplayType_CLILCD:
    {
        b = NvDdkDispPrivUpdate( hDisplay, hWindow, surf, count, pSrc,
            pUpdate, stream, fence, flags );
        if( !b )
        {
            goto fail;
        }
        break;
    }
    default:
        goto fail;
    }

    e = NvSuccess;
    goto clean;

fail:
    e = NvError_BadParameter;

clean:
    NvOsMutexUnlock( hDisplay->mutex );
    return e;
}

NvError
NvDdkDispGetBestMode(
    NvDdkDispControllerHandle hController,
    NvDdkDispMode *pMode,
    NvU32 flags)
{
    NvError err;
    NvBool b;
    NV_ASSERT( hController );
    NV_ASSERT( pMode );
    NV_ASSERT( flags == 0 );

    NvOsMutexLock( hController->mutex );

    if( !hController->nAttached )
    {
        err = NvError_DispNoDisplaysAttached;
        goto clean;
    }

    if( hController->nAttached == 1 )
    {
        NvU32 i;
        NvDdkDispMode *m;
        NvDdkDispMode *q = 0;
        NvDdkDispDisplayHandle hDisplay;
        NvBool bFound = NV_FALSE;
        NvS32 freq = 0, max_freq = 0;

        hDisplay = hController->AttachedDisplays[0];

        /* maybe the panel supports plug-n-play */
        b = NvDdkDispPrivEdid( hDisplay );

        max_freq = NvDdkDispPrivGetMaxFrequency( hDisplay );

        /* look for a native mode, or the biggest one that fits */
        for( i = 0; i < hDisplay->panel->nModes; i++ )
        {
            m = &hDisplay->panel->modes[i];

            if( m->frequency > max_freq )
            {
                continue;
            }

            if( m->flags & NVDDK_DISP_MODE_FLAG_NATIVE )
            {
                *pMode = *m;
                bFound = NV_TRUE;
                break;
            }

            if( !q )
                q = m;

            if( b )
            {
                if (q->frequency < m->frequency)
                    q = m;
            }
            else
            {
                if (q->frequency > m->frequency)
                    q = m;
            }

            /* filter for the hdmi modes if this is an hdmi tv */
            if( hDisplay->attr.Type == NvDdkDispDisplayType_HDMI &&
                (m->flags & NVDDK_DISP_MODE_FLAG_TYPE_HDMI) == 0 )
            {
                continue;
            }

            if( m->frequency > freq )
            {
                /* don't break out of the loop -- find the biggest one! */
                freq = m->frequency;
                *pMode = *m;
                bFound = NV_TRUE;
            }
        }

        if( !bFound )
        {
            if( q )
            {
                *pMode = *q;
            }
            else
            {
                err = NvError_DispModeNotSupported;
                goto clean;
            }
        }
    }
    else
    {
        // FIXME: handle multiple attached displays
    }

    err = NvSuccess;

clean:
    NvOsMutexUnlock( hController->mutex );

    return err;
}

NvError
NvDdkDispGetMode(
    NvDdkDispControllerHandle hController,
    NvDdkDispMode *pMode)
{
    NV_ASSERT( hController );
    NV_ASSERT( pMode );

    *pMode = hController->mode;

    return NvSuccess;
}

/**
 * For all attached displays, verify compatible modes.  Assumes that
 * the hController mutex is held. The mode and set attributes must comply with
 * the physical display device's features (can't use a feature that the panel
 * does not support).
 */
static NvBool
NvDdkDispPrivVerifyMode( NvDdkDispControllerHandle hController,
    NvDdkDispMode *mode )
{
    NvS32 max_freq;
    NvDdkDispDisplayHandle hDisplay;

    if( !hController->nAttached )
    {
        return NV_FALSE;
    }

    hDisplay = hController->AttachedDisplays[0];
    max_freq = NvDdkDispPrivGetMaxFrequency( hDisplay );

    if( mode->frequency && mode->frequency > max_freq )
    {
        return NV_FALSE;
    }

    // FIXME: finish implementation - should handle multiply-attached
    // displays, etc.
    return NV_TRUE;
}

/**
 * compares two modes.  handles a null mode compare against a zero'd mode.
 */
static NvBool
NvDdkDispPrivModeCompare( NvOdmDispDeviceMode *m1, NvOdmDispDeviceMode *m2 )
{
    if( m1 && m2 )
    {
        if( m1->bpp == m2->bpp &&
            m1->width == m2->width &&
            m1->height == m2->height &&
            m1->flags == m2->flags )
        {
            return NV_TRUE;
        }
    }
    else if( !m1 && m2 && NvDdkDispPrivIsModeNull( m2 ) )
    {
        return NV_TRUE;
    }
    else if( !m2 && m1 && NvDdkDispPrivIsModeNull( m1 ) )
    {
        return NV_TRUE;
    }

    return NV_FALSE;
}

/** assumes hal->HwBegin() has been called */
static void
NvDdkDispPrivCommitWindow( NvDdkDispWindowHandle hWindow )
{
    NvDdkDispHal *hal;

    hal = &hWindow->hController->hal;

    hal->HwSetDepth( hWindow->hController->Index, hWindow->attr.Number,
        hWindow->attr.Depth );
    hal->HwSetFiltering( hWindow->hController->Index,
        hWindow->attr.Number, hWindow->attr.HorizFiltering,
        hWindow->attr.VertFiltering );
    hal->HwSetVibrance( hWindow->hController->Index,
        hWindow->attr.Number, hWindow->attr.DigitalVibrance );
    hal->HwSetScaleNicely( hWindow->hController->Index,
        hWindow->attr.Number, hWindow->attr.ScaleNicely );
    hal->HwSetBlend( hWindow->hController->Index, hWindow->attr.Number,
        hWindow->attr.BlendType );
    hal->HwSetAlphaOp( hWindow->hController->Index, hWindow->attr.Number,
        hWindow->attr.AlphaOperation, hWindow->attr.AlphaDirection );
    hal->HwSetAlpha( hWindow->hController->Index, hWindow->attr.Number,
        hWindow->attr.AlphaValue );
    hal->HwSetCsc( hWindow->hController->Index, hWindow->attr.Number,
        &hWindow->attr.ColorSpaceCoef );

    /* make sure the color key is correct, write to hardware */
    if( hWindow->nSurfaces )
    {
        NvDdkDispPrivColorKey( hWindow );
    }

    hWindow->bDirty = NV_FALSE;
}

/** assumes hal->HwBegin() has been called */
static void
NvDdkDispPrivCommitDisplay( NvDdkDispDisplayHandle hDisplay )
{
    NvDdkDispHal *hal;
    NvDdkDispPanel *panel;
    NvU32 num;
    NvU32 nPins;
    NvOdmDispPin pins[NvOdmDispPin_Num];
    NvOdmDispPinPolarity polarities[NvOdmDispPin_Num];

    hal = &hDisplay->CurrentController->hal;
    panel = hDisplay->panel;
    num = hDisplay->CurrentController->Index;

    hal->HwSetColorCorrection( num, hDisplay->attr.Brightness,
        hDisplay->attr.GammaRed, hDisplay->attr.GammaGreen,
        hDisplay->attr.GammaBlue, hDisplay->attr.ScaleRed,
        hDisplay->attr.ScaleGreen, hDisplay->attr.ScaleBlue,
        hDisplay->attr.Contrast );

    if( panel->BaseColorSize )
    {
        hal->HwSetBaseColorSize( num, panel->BaseColorSize );
    }
    if( panel->DataAlignment )
    {
        hal->HwSetDataAlignment( num, panel->DataAlignment );
    }
    if( panel->PinMap )
    {
        hal->HwSetPinMap( num, panel->PinMap );
    }
    if( panel->PwmOutputPin )
    {
        hal->HwSetPwmOutputPin( num, panel->PwmOutputPin );
    }
    if( panel->builtin && panel->builtin->AudioFrequency )
    {
        panel->builtin->AudioFrequency( num, hDisplay->attr.AudioFrequency );
    }

    if( hDisplay->attr.Type == NvDdkDispDisplayType_TV )
    {
        DisplayAttr *attr;
        attr = &hDisplay->attr;

        hal->TvHal->HwTvType( hDisplay->attr.TV_Type );
        hal->TvHal->HwTvAttrs( hDisplay->CurrentController->Index,
            attr->TV_Overscan, attr->TV_OverscanY, attr->TV_OverscanCb,
            attr->TV_OverscanCr, attr->TV_OutputFormat,
            attr->TV_DAC_Amplitude, attr->TV_FilterMode,
            attr->TV_ScreenFormat);
        hal->TvHal->HwTvPosition( hDisplay->CurrentController->Index,
            attr->TV_HorizontalPosition, attr->TV_VerticalPosition);
    }

    /* backlight is handled elsewhere (except for the AutoBacklight special
     * function)
     */
    if( panel->hPanel )
    {
        NvU8 AutoBacklightVal;
        NvOdmDispPartialModeConfig cfg;

        AutoBacklightVal = (NvU8)hDisplay->attr.AutoBacklight;
        NvOdmDispSetSpecialFunction(panel->hPanel,
                   NvOdmDispSpecialFunction_AutoBacklight, &AutoBacklightVal);

        NvOdmDispGetPinPolarities( panel->hPanel, &nPins, pins, polarities );
        if( nPins )
        {
            hal->HwSetPinPolarities( num, nPins, pins, polarities );
        }

        cfg.LineOffset = hDisplay->attr.PartialModeLineOffset;
        NvOdmDispSetSpecialFunction( panel->hPanel,
            NvOdmDispSpecialFunction_PartialMode, &cfg );
    }

    if( hDisplay->attr.Type == NvDdkDispDisplayType_CLILCD )
    {
        NvOdmDispCliConfig *cfg;
        cfg = (NvOdmDispCliConfig *)panel->Config;
        if( cfg->nInstructions )
        {
            hal->HwSetPeriodicInstructions( num, cfg->nInstructions,
                cfg->Instructions );
        }
    }
}

/** assumes hal->HwBegin() has been called */
static void
NvDdkDispPrivCommitController( NvDdkDispControllerHandle
    hController )
{
    NvDdkDispHal *hal;
    NvDdkDispCapabilities *caps = 0;

    NvBool sd = NV_FALSE;

    hal = &hController->hal;
    caps = hController->hDisp->caps;

    hal->HwSetBackgroundColor( hController->Index,
        hController->attr.BackgroundColor );

    if( hController->cursor.enable )
    {
        NV_ASSERT(hController->cursor.current);
        hal->HwSetCursor( hController->Index,
            *hController->cursor.current,
            &hController->cursor.size,
            hController->cursor.fgColor,
            hController->cursor.bgColor );
        hal->HwSetCursorPosition( hController->Index,
            &hController->cursor.position );
        // HW pins the memory - unpin old (if present) and mark new pinned
        if ( hController->cursor.pinned )
            NvRmMemUnpin( *hController->cursor.pinned );
        hController->cursor.pinned = hController->cursor.current;
    }
    else
    {
        hal->HwSetCursorPosition( hController->Index, 0 );
    }

    if( hController->error_thread )
    {
        hal->HwSetErrorSem( hController->Index,
            hController->error_thread->sem );
    }
    sd = NVDDK_DISP_CAP_IS_SET( caps, NvDdkDispCapBit_SmartDimmer ) ;

    if( sd )
    {
        NvBool manual;
        NvDdkSmartDimmerSettings sdSettings;
        sdSettings.Enable = hController->attr.SmartDimmer_Enable;
        sdSettings.BinWidth = hController->attr.SmartDimmer_BinWidth;
        sdSettings.Aggressiveness = hController->attr.SmartDimmer_Aggressiveness;
        sdSettings.HwUpdateDelay = hController->attr.SmartDimmer_HwUpdateDelay;
        sdSettings.bEnableVidLum = hController->attr.SmartDimmer_VideoLuminance;
        sdSettings.Coeff_R = hController->attr.SmartDimmer_Csc_Coeff_R;
        sdSettings.Coeff_G = hController->attr.SmartDimmer_Csc_Coeff_G;
        sdSettings.Coeff_B = hController->attr.SmartDimmer_Csc_Coeff_B;
        sdSettings.bEnableManK = hController->attr.SmartDimmer_Man_K_Enable;
        sdSettings.ManK_R = hController->attr.SmartDimmer_Man_K_R;
        sdSettings.ManK_G = hController->attr.SmartDimmer_Man_K_G;
        sdSettings.ManK_B = hController->attr.SmartDimmer_Man_K_B;
        sdSettings.FlickerTimeLimit = hController->attr.SmartDimmer_FlickerTimeLimit;
        sdSettings.FlickerThreshold = hController->attr.SmartDimmer_FlickerThreshold;
        sdSettings.BlStep = hController->attr.SmartDimmer_BlStep;
        sdSettings.BlTimeConstant = hController->attr.SmartDimmer_BlTimeConstant;

        hal->HwSmartDimmerConfig( hController->Index,
            &sdSettings, &manual );

        if( manual && hController->attr.SmartDimmer_Enable &&
            hController->smartdimmer_thread == 0 )
        {
            NvBool b;
            b = NvDdkDispPrivCreateSmartDimmerThread( hController );
            if( !b )
            {
                // Zero out the struct to reset everything.
                NvOsMemset(&sdSettings, 0, sizeof(sdSettings));
                hal->HwSmartDimmerConfig( hController->Index,
                    &sdSettings, NULL );
            }
        }
        else if( hController->attr.SmartDimmer_Enable &&
            hController->smartdimmer_thread )
        {
            /* send a signal to wake the thread back up */
            NvOsSemaphoreSignal( hController->smartdimmer_thread->sem );
        }
    }

    hal->HwTestCrcEnable( hController->Index, hController->CrcEnabled );
}

/**
 * Commits all controller, window, and attached display attributes.
 * For each display, enables the physical display device via the odm.
 *
 * This commits everything except the backlight so that the mode can be set
 * before the backlight is enabled (looks better).
 */
static NvBool
NvDdkDispPrivCommitAllAttributes( NvDdkDispControllerHandle hController )
{
    NvDdkDispDisplayHandle hDisplay;
    NvDdkDispWindowHandle hWindow;
    NvDdkDispCapabilities *caps;
    NvU32 i;
    NvDdkDispHal *hal;

    NV_ASSERT( hController );
    NV_ASSERT( hController->nAttached );

    caps = hController->hDisp->caps;

    // FIXME: lock the displays -- lock order issue since controller is
    // already locked.

    hal = &hController->hal;

    hal->HwBegin( hController->Index );

    for( i = 0; i < caps->NumWindows; i++ )
    {
        hWindow = &hController->windows[i];
        NvDdkDispPrivCommitWindow( hWindow );
    }

    for( i = 0; i < hController->nAttached; i++ )
    {
        hDisplay = hController->AttachedDisplays[i];
        NvDdkDispPrivCommitDisplay( hDisplay );
    }

    NvDdkDispPrivCommitController( hController );

    return hal->HwEnd( hController->Index, 0 );
}

static NvBool NvDdkDispPrivIsSurfaceNull( NvRmSurface *surf )
{
    if( surf->hMem || surf->pBase )
    {
        return NV_FALSE;
    }

    return NV_TRUE;
}

/**
 * Update window hardware (actually display something).
 * Assumes controller mutex is held.
 * Assumes HAL begin/end is handled by the caller.
 */
static void
NvDdkDispPrivUpdateSurface( NvDdkDispControllerHandle hController,
    NvDdkDispWindowHandle first, NvDdkDispWindowHandle last,
    NvBool *enabled )
{
    NvDdkDispHal *hal;
    NvBool bEnable = NV_FALSE;
    NvDdkDispWindowHandle w;

    hal = &hController->hal;

    w = first;
    for(;;)
    {
        NvRect *src = &w->attr.SourceRect;

        /* default source rect to be the same as dest rect */
        if( !src->bottom && !src->right && !src->left && !src->top )
        {
            src->bottom = w->attr.DestRect.bottom;
            src->right = w->attr.DestRect.right;
            src->left = w->attr.DestRect.left;
            src->top = w->attr.DestRect.top;
        }

        hal->HwSetWindowSurface( hController->Index,
            w->attr.Number, w->surface, w->nSurfaces, &w->attr.SourceRect,
            &w->attr.DestRect, &w->attr.Location, w->attr.Rotation,
            w->attr.Mirror );

        if( !NvDdkDispPrivIsSurfaceNull( &w->surface[0] ) )
        {
            bEnable = NV_TRUE;
        }

        if( w == last )
        {
            break;
        }
        w++;
    }

    *enabled = bEnable;
}

/**
 * assumes that the HAL begin()/end() are handled by the caller.
 */
static void
NvDdkDispPrivUpdatePanels( NvDdkDispControllerHandle hController,
    NvOdmDispDevicePowerLevel level )
{
    NvDdkDispHandle hDisp;
    NvDdkDispDisplayHandle hDisplay = 0;
    NvDdkDispHal *hal;
    NvDdkDispPanel *panel;
    NvOdmDispDeviceMode *mode = 0;
    NvBool bReopen = NV_FALSE;
    NvBool enable = NV_FALSE;
    NvU32 i;

    hal = &hController->hal;
    hDisp = hController->hDisp;
    mode = &hController->mode;

    /* state machine
     *
     * Off -> PrePower: call NvOdmDispSetMode then NvOdmDispPowerLevel
     * Any -> Off: call NvOdmDispPowerLevel then NvOdmDispSetMode
     * Any -> On, On -> [SelfRefresh|Suspend]: call NvOdmDispPowerLevel
     */

    #define ENABLE( en ) \
        do { \
            if (panel->builtin) \
            { \
                panel->builtin->Enable( hController->Index, (en) ); \
            } \
            else if (hDisplay->panel->Type == NvOdmDispDeviceType_DSI) \
            { \
                hal->HwDsiEnable( hController->Index, panel->hPanel, \
                    hDisplay->panel->DsiInstance, (en) ); \
            } \
        } while( 0 )

    for( i = 0; i < hController->nAttached; i++ )
    {
        hDisplay = hController->AttachedDisplays[i];
        panel = hDisplay->panel;

        // FIXME: need set-mode function pointer for the builtin types.

        /* allow setting a mode when display is active */
        if( hDisplay->power == NvOdmDispDevicePowerLevel_On &&
            hDisplay->power == level &&
            panel->hPanel &&
            hController->bModeDirty )
        {
            if (hDisplay->panel->Type == NvOdmDispDeviceType_DSI)
            {
                hal->HwDsiSwitchMode(hController->hDisp->hDevice,
                    hController->Index, hDisplay->panel->DsiInstance,
                    panel->hPanel );
            }
            else
            {
                NvOdmDispSetMode( panel->hPanel, mode, NV_FALSE );
            }
            hController->bModeDirty = NV_FALSE;
        }

        if( hDisplay->power == level )
        {
            continue;
        }

        switch( level ) {
        case NvOdmDispDevicePowerLevel_Off:
            NvDdkDispPrivSetPowerLevel( hDisplay, level );

            ENABLE( NV_FALSE );

            if( panel->hPanel )
            {
                NvOdmDispSetMode( panel->hPanel, 0, 0 );
            }

            continue;
        case NvOdmDispDevicePowerLevel_PrePower:
            NV_ASSERT( hDisplay->power == NvOdmDispDevicePowerLevel_Off ||
                hDisplay->power == NvOdmDispDevicePowerLevel_Suspend ||
                hDisplay->power == NvOdmDispDevicePowerLevel_SelfRefresh );

            if (hDisplay->panel->Type == NvOdmDispDeviceType_DSI)
            {
                const NvOdmDispDsiConfig *info;
                info = (NvOdmDispDsiConfig *)hDisplay->panel->Config;
                if (info->DsiDisplayMode == NvOdmDispDsiMode_Ganged)
                {
                    //Both DSI instances use same DC instance
                    hal->HwDsiSetIndex( hController->Index,
                        1, panel->hPanel );
                    hal->HwDsiSetIndex( hController->Index,
                        0, panel->hPanel );
                }
                else
                    hal->HwDsiSetIndex( hController->Index,
                        hDisplay->panel->DsiInstance, panel->hPanel );
            }

            if( panel->hPanel )
            {
                NvOdmDispSetMode( panel->hPanel, mode, bReopen );
            }

            NvDdkDispPrivSetPowerLevel( hDisplay, level );
            continue;
        case NvOdmDispDevicePowerLevel_On:
            enable = NV_TRUE;
            break;
        case NvOdmDispDevicePowerLevel_Suspend:
        case NvOdmDispDevicePowerLevel_SelfRefresh:
            enable = NV_FALSE;
            break;
        default:
            NV_ASSERT( !"bad power level" );
            return;
        }

        NvDdkDispPrivSetPowerLevel( hDisplay, level );
        ENABLE( enable );
    }

    #undef ENABLE

    if( bReopen )
    {
        hDisp->bReopen = NV_FALSE;
    }
}

NvError
NvDdkDispSetMode(
    NvDdkDispControllerHandle hController,
    NvDdkDispMode *pMode,
    NvU32 flags)
{
    NvError err;
    NvDdkDispMode mode;
    NvOdmDispDeviceMode *om;
    NvDdkDispHal *hal;
    NvBool b;

    NV_ASSERT( hController );

    if( !flags )
    {
        flags = NVDDK_DISP_SETMODE_ALWAYS | NVDDK_DISP_WAIT;
    }
    else if( flags & ~NVDDK_DISP_SETMODE_ALWAYS & ~NVDDK_DISP_SETMODE_SHADOW &
        ~NVDDK_DISP_SETMODE_BEST & ~NVDDK_DISP_WAIT & ~NVDDK_DISP_DEBUG )
    {
        return NvError_BadParameter;
    }

    if( !pMode && ( flags & NVDDK_DISP_SETMODE_BEST ) )
    {
        pMode = &mode;
        err = NvDdkDispGetBestMode( hController, pMode, 0 );
        if( err != NvSuccess )
        {
            return NvError_DispModeNotSupported;
        }
    }

    NvOsMutexLock( hController->mutex );

    hal = &hController->hal;

    if( pMode && !hController->nAttached )
    {
        err = NvError_DispNoDisplaysAttached;
        goto clean;
    }

    if( pMode )
    {
        /* verify that the mode is supported by all attached displays */
        if( !NvDdkDispPrivVerifyMode( hController, pMode ) )
        {
            err = NvError_DispModeNotSupported;
            goto clean;
        }

        NvDdkDispPrivSanitizeMode( pMode );
        om = pMode;

        /* is the the same mode? NvDdkDispAttachDisplay can call this
         * for a re-attach, need to check if the current state is stopped, etc.
         */
        if( hController->state == ControllerState_Active &&
            NvDdkDispPrivModeCompare( &hController->mode, om ) )
        {
            /* don't do anything */
            err = NvSuccess;
            goto clean;
        }

        hController->mode = *om;
        hController->bModeDirty = NV_TRUE;

        if( flags & NVDDK_DISP_DEBUG )
        {
            NvOsDebugPrintf( "NvDdkDispSetMode/ controller: %d\n",
                hController->Index );
            NvDdkDispPrivPrintMode( om );
        }

        /* write the mode to hardware */
        if( hController->window_enable &&
            hController->state != ControllerState_Suspend )
        {
            b = NvDdkDispPrivModeCommit( hController );
            if( !b )
            {
                goto fail;
            }
        }
    }
    else
    {
        if( hController->state == ControllerState_Active )
        {
            /* disable the panels */
            hal->HwBegin( hController->Index );

            NvDdkDispPrivUpdatePanels( hController,
                NvOdmDispDevicePowerLevel_Off );

            b = hal->HwEnd( hController->Index, 0 );
            if( !b )
            {
                goto fail;
            }

            /* disable display controller hardware */
            hal->HwClose( hController->Index );
            hController->HwInitialized = NV_FALSE;
        }

        if( flags & NVDDK_DISP_DEBUG )
        {
            NvOsDebugPrintf( "NvDdkDispSetMode/ null mode\n" );
        }

        NvOsMemset( &hController->mode, 0, sizeof(hController->mode) );
        hController->state = ControllerState_Stopped;
        b = NvDdkDispPrivPowerHint( hController->hDisp );
        if( !b )
        {
            goto fail;
        }
    }

    err = NvSuccess;
    goto clean;

fail:
    err = NvError_BadParameter;

clean:
    NvOsMutexUnlock( hController->mutex );

    return err;
}

static void NvDdkDispPrivAssignSurface( NvRmSurface *dst, NvRmSurface *src )
{
    NV_ASSERT( dst );

    if( !src )
    {
        NvOsMemset( dst, 0, sizeof(NvRmSurface) );
    }
    else
    {
        *dst = *src;
    }
}

NvError
NvDdkDispSetWindowSurface(
    NvDdkDispWindowHandle hWindow,
    NvRmSurface *surf,
    NvU32 count,
    NvU32 flags)
{
    NvError err = NvSuccess;
    NvDdkDispControllerHandle hController;
    NvDdkDispHal *hal;
    NvOdmDispDevicePowerLevel level;
    NvU32 i;
    NvBool bUpdatePanel = NV_FALSE;
    NvBool bUpdateBacklight = NV_FALSE;
    NvBool bControllerStopped = NV_FALSE;
    NvBool b;

    NV_ASSERT( hWindow );
    NV_ASSERT( flags == 0 ||
        !( (flags & ~NVDDK_DISP_WAIT
                  & ~NVDDK_DISP_DO_NOT_COMMIT
                  & ~NVDDK_DISP_SURFACE_LEAVEON
                  & ~NVDDK_DISP_DEBUG )));

    hController = hWindow->hController;

    NvOsMutexLock( hController->mutex );

    NV_DEBUG_CODE(
        if( !count )
        {
            NV_ASSERT( !surf );
        }
    );

    if( !surf )
    {
        count = 1;

        if( NvDdkDispPrivIsSurfaceNull( &hWindow->surface[0] ) )
        {
            /* nothing to do, already null */
            err = NvSuccess;
            goto clean;
        }
    }

    if( flags & NVDDK_DISP_DO_NOT_COMMIT )
    {
        hWindow->bDirty = NV_TRUE;
    }

    if( flags & NVDDK_DISP_DEBUG )
    {
        NvOsDebugPrintf( "NvDdkDispSetWindowSurface/ controller: %d "
            "window: %d count: %d\n", hWindow->hController->Index,
            hWindow->attr.Number, count );

        for( i = 0; i < count; i++ )
        {
            if( !surf )
            {
                NvOsDebugPrintf( "surface: 0\n" );
                break;
            }

            NvDdkDispPrivPrintSurface( &surf[i] );
        }
    }

    if( NvDdkDispPrivIsModeNull( &hController->mode ) ||
        hController->nAttached == 0 ||
        hController->state == ControllerState_Suspend ||
        (flags & NVDDK_DISP_DO_NOT_COMMIT) )
    {
        for( i = 0; i < count; i++ )
        {
            NvDdkDispPrivAssignSurface( &hWindow->surface[i], surf + i );
        }

        hWindow->nSurfaces = (surf) ? count : 0;

        if( surf )
        {
            hController->window_enable |= hWindow->mask;
        }
        else
        {
            hController->window_enable &= ~(hWindow->mask);
        }

        err = NvSuccess;
        goto clean;
    }

    hal = &hController->hal;

    /* A mode has been set and there is a display attached. The hardware
     * will be updated (or activated) and actually display something.
     */

    /* if the window is being activated (surface will become non-null),
     * commit all of the window attributes to hardware.
     */
    if( (NvDdkDispPrivIsSurfaceNull( &hWindow->surface[0] ) && surf) ||
        (hController->state != ControllerState_Active) )
    {
        if( hController->state != ControllerState_Active )
        {
            /* restart the controller */
            b = NvDdkDispPrivModeCommit( hController );
            if( !b )
            {
                goto fail;
            }
        }

        bUpdateBacklight = NV_TRUE;
        hal->HwBegin( hController->Index );
        NvDdkDispPrivCommitWindow( hWindow );

        if( !hController->window_enable )
        {
            bUpdatePanel = NV_TRUE;
        }

        hController->window_enable |= hWindow->mask;

        b = hal->HwEnd( hController->Index, 0 );
        if( !b )
        {
            goto fail;
        }
    }
    else if( !surf )
    {
        hController->window_enable &= ~(hWindow->mask);
        if( !hController->window_enable )
        {
            if( (flags & NVDDK_DISP_SURFACE_LEAVEON) !=
                NVDDK_DISP_SURFACE_LEAVEON )
            {
                /* disable the window hardware */
                bUpdatePanel = NV_TRUE;
                bControllerStopped = NV_TRUE;
            }
        }
    }

    for( i = 0; i < count; i++ )
    {
        NvDdkDispPrivAssignSurface( &hWindow->surface[i], surf + i );
    }
    hWindow->nSurfaces = (surf) ? count : 0;

    /* this is a power event */
    b = NvDdkDispPrivPowerHint( hController->hDisp );
    if( !b )
    {
        goto fail;
    }

    /* begin hardware write sequence */
    hal->HwBegin( hController->Index );

    if( hWindow->bDirty )
    {
        NvDdkDispPrivCommitWindow( hWindow );
    }

    /* update the window hardware (just one window) */
    NvDdkDispPrivUpdateSurface( hController, hWindow, hWindow, &b );

    /* enable/disable all panels */
    if( bUpdatePanel )
    {
        level = ( hController->window_enable ) ? NvOdmDispDevicePowerLevel_On :
            NvOdmDispDevicePowerLevel_Off;
        NvDdkDispPrivUpdatePanels( hController, level );
    }

    if( hWindow->bColorKeyDirty )
    {
        NvDdkDispPrivColorKey( hWindow );
    }

    /* commit to hardware */
    b = hal->HwEnd( hController->Index, flags );
    if( !b )
    {
        goto fail;
    }

    /* check for one-shot updates - should send a frame before enabling
     * backlight to avoid uninitialized framebuffer issues.
     */
    if( surf &&
        (hController->mode.flags & NVDDK_DISP_MODE_FLAG_USE_TEARING_EFFECT) )
    {
        hal->HwTriggerFrame( hController->Index );
    }

    if( bUpdateBacklight )
    {
        hal->HwBegin( hController->Index );
        NvDdkDispPrivCommitBacklight( hController );
        b = hal->HwEnd( hController->Index, flags );
        if( !b )
        {
            goto fail;
        }
    }

    if( bControllerStopped )
    {
        hController->state = ControllerState_Stopped;
        hController->HwInitialized = NV_FALSE;
        hal->HwClose( hController->Index );
    }

    err = NvSuccess;
    goto clean;

fail:
    err = NvError_BadParameter;
    NvOsMemset( &hWindow->surface[0], 0, sizeof(NvRmSurface) );
    hWindow->nSurfaces = 0;

clean:
    NvOsMutexUnlock( hController->mutex );
    return err;
}

NvError
NvDdkDispGetWindowSurface(
    NvDdkDispWindowHandle hWindow,
    NvRmSurface *surf,
    NvU32 *count )
{
    NV_ASSERT( hWindow );
    NV_ASSERT( count );
    NV_ASSERT( *count <= NVDDK_DISP_MAX_SURFACES );

    NvOsMutexLock( hWindow->hController->mutex );

    if( surf )
    {
        NvU32 i;

        for( i = 0; i < *count; i++ )
        {
            surf[i] = hWindow->surface[i];
        }
    }

    *count = hWindow->nSurfaces;

    NvOsMutexUnlock( hWindow->hController->mutex );

    return NvSuccess;
}

NvError
NvDdkDispFlipWindowSurface(
    NvDdkDispWindowHandle hWindow,
    NvRmSurface *surf,
    NvU32 count,
    NvRmStream *stream,
    NvRmFence *fence,
    NvU32 flags )
{
    NvError err;
    NvDdkDispHal *hal;
    NvU32 i;

    NV_ASSERT( hWindow );
    NV_ASSERT( surf );
    NV_ASSERT( count );
    NV_ASSERT( (flags & ~(NVDDK_DISP_FLIP_WAIT_FENCE |
                          NVDDK_DISP_FLIP_NO_VSYNC_FENCE |
                          NVDDK_DISP_WAIT)) == 0 );

    NvOsMutexLock( hWindow->hController->mutex );

    /* verify that a surface has already been setup and the controller is
     * active
     */
    if( NvDdkDispPrivIsSurfaceNull( &hWindow->surface[0] ) ||
        hWindow->nSurfaces != count ||
        hWindow->hController->state != ControllerState_Active )
    {
        /* go ahead and set the surface the hard way. fill in an invalid
         * sync point id to flag nothing-to-wait-for.
         */
        err = NvDdkDispSetWindowSurface( hWindow, surf, count,
            NVDDK_DISP_WAIT );
        if( err != NvSuccess )
        {
            goto clean;
        }

        if( fence )
        {
            fence->SyncPointID = NVRM_INVALID_SYNCPOINT_ID;
            fence->Value = 0;
        }
    }
    else
    {
        NvDdkDispControllerHandle hController = hWindow->hController;
        NvBool use_te;
        use_te = (hController->mode.flags &
            NVDDK_DISP_MODE_FLAG_USE_TEARING_EFFECT) ? NV_TRUE : NV_FALSE;

        /* save the surfaces */
        for( i = 0; i < count; i++ )
        {
            NvDdkDispPrivAssignSurface( &hWindow->surface[i], surf + i );
        }

        /* do the surface flip. no begin/end required */
        hal = &hWindow->hController->hal;
        hal->HwFlipWindowSurface( hController->Index,
            hWindow->attr.Number, surf, count, stream, fence, flags );

        /* check for TE */
        if( use_te )
        {
            if( !stream || (flags & NVDDK_DISP_FLIP_NO_VSYNC_FENCE) )
                hal->HwTriggerFrame( hController->Index );
        }
    }

    err = NvSuccess;

clean:
    NvOsMutexUnlock( hWindow->hController->mutex );
    return err;
}

NvBool
NvDdkDispWindowSupportsFormat(
    NvDdkDispWindowHandle hWindow,
    NvColorFormat format )
{
    NvDdkDispCapabilities *caps = 0;
    NvDdkDispWindowCap *wincap = 0;
    NvColorFormat *fmts = 0;

    NV_ASSERT( hWindow );
    NV_ASSERT( hWindow->hController );
    NV_ASSERT( hWindow->hController->hDisp );

    caps = hWindow->hController->hDisp->caps;
    NV_ASSERT( caps );
    NV_ASSERT( hWindow->attr.Number < caps->NumWindows );

    wincap = &caps->WindowCaps[hWindow->attr.Number];
    NV_ASSERT( wincap );

    fmts = wincap->formats;
    while( *fmts )
    {
        if( *fmts == format )
        {
            return NV_TRUE;
        }

        fmts++;
    }

    return NV_FALSE;
}

NvBool
NvDdkDispWindowSupportsSurface(
    NvDdkDispWindowHandle hWindow,
    NvRmSurface *surfs,
    NvU32 num )
{
    NV_ASSERT( hWindow );
    NV_ASSERT( surfs );
    NV_ASSERT( num );

    if( num == 1 )
    {
        return NvDdkDispWindowSupportsFormat( hWindow, surfs->ColorFormat );
    }

    if( num == 3 )
    {
        NvYuvColorFormat format;
        NvDdkDispCapabilities *caps = 0;
        NvDdkDispWindowCap *wincap = 0;
        NvYuvColorFormat *fmts = 0;
        NvRmSurface *s[3];

        caps = hWindow->hController->hDisp->caps;
        NV_ASSERT( caps );

        wincap = &caps->WindowCaps[hWindow->attr.Number];
        NV_ASSERT( wincap );

        if( surfs[1].ColorFormat != NvColorFormat_U8 ||
            surfs[2].ColorFormat != NvColorFormat_V8 )
        {
            return NV_FALSE;
        }

        s[0] = &surfs[0];
        s[1] = &surfs[1];
        s[2] = &surfs[2];

        format = NvRmSurfaceGetYuvColorFormat( s, 3 );
        fmts = wincap->yuv_formats;
        while( fmts && *fmts )
        {
            if( *fmts == format )
            {
                return NV_TRUE;
            }

            fmts++;
        }
    }

    return NV_FALSE;
}

static void
NvDdkDispPrivColorKey( NvDdkDispWindowHandle hWindow )
{
    NvDdkDispHal *hal;
    NvU32 r, g, b;

    #define PIXEL_COMPONENTS_565( pix, r, g, b ) \
        do { \
            (b) = (pix) & 0x1f; \
            (g) = ( (pix) & ( 0x3f << 5 ) ) >> 5; \
            (r) = ( (pix) & ( 0x1f << 11 ) ) >> 11; \
        } while( 0 )

    #define PIXEL_COMPONENTS_A8B8G8R8( pix, r, g, b ) \
        do { \
            (r) = (pix) & 0xff; \
            (g) = ( (pix) & ( 0xff << 8 ) ) >> 8; \
            (b) = ( (pix) & ( 0xff << 16 ) ) >> 16; \
        } while( 0 )

    #define PIXEL_COMPONENTS_R8G8B8A8( pix, r, g, b ) \
        do { \
            (r) = ( (pix) & ( 0xff << 24 ) ) >> 24; \
            (g) = ( (pix) & ( 0xff << 16 ) ) >> 16; \
            (b) = ( (pix) & ( 0xff <<  8 ) ) >>  8; \
        } while( 0 )

    #define PIXEL_COMPONENTS_B8G8R8A8( pix, r, g, b ) \
        do { \
            (b) = ( (pix) & ( 0xff << 24 ) ) >> 24; \
            (g) = ( (pix) & ( 0xff << 16 ) ) >> 16; \
            (r) = ( (pix) & ( 0xff <<  8 ) ) >>  8; \
        } while( 0 )

    if( hWindow->attr.BlendType != NvDdkDispBlendType_ColorKey )
    {
        return;
    }

    /* color key is specified in the surface's color format, need to convert
     * to A8R8G8B8. ignore yuv formats (always specified in A8R8G8B8).
     */
    if( hWindow->bColorKeyDirty )
    {
        // FIXME: add more formats
        switch( hWindow->surface[0].ColorFormat ) {
        case NvColorFormat_A8B8G8R8:
            PIXEL_COMPONENTS_A8B8G8R8( hWindow->attr.ColorKeyUpper, r, g, b );
            hWindow->attr.ColorKeyUpper = (r << 16) | (g << 8) | b;
            PIXEL_COMPONENTS_A8B8G8R8( hWindow->attr.ColorKeyLower, r, g, b );
            hWindow->attr.ColorKeyLower = (r << 16) | (g << 8) | b;
            break;
        case NvColorFormat_R8G8B8A8:
            PIXEL_COMPONENTS_R8G8B8A8( hWindow->attr.ColorKeyUpper, r, g, b );
            hWindow->attr.ColorKeyUpper = (r << 16) | (g << 8) | b;
            PIXEL_COMPONENTS_R8G8B8A8( hWindow->attr.ColorKeyLower, r, g, b );
            hWindow->attr.ColorKeyLower = (r << 16) | (g << 8) | b;
            break;
        case NvColorFormat_B8G8R8A8:
            PIXEL_COMPONENTS_B8G8R8A8( hWindow->attr.ColorKeyUpper, r, g, b );
            hWindow->attr.ColorKeyUpper = (r << 16) | (g << 8) | b;
            PIXEL_COMPONENTS_B8G8R8A8( hWindow->attr.ColorKeyLower, r, g, b );
            hWindow->attr.ColorKeyLower = (r << 16) | (g << 8) | b;
            break;
        case NvColorFormat_R5G6B5:
        {
            PIXEL_COMPONENTS_565( hWindow->attr.ColorKeyUpper, r, g, b );
            r = r << 3;
            g = g << 2;
            b = b << 3;
            hWindow->attr.ColorKeyUpper = (r << 16) | (g << 8) | b;

            PIXEL_COMPONENTS_565( hWindow->attr.ColorKeyLower, r, g, b );
            r = r << 3;
            g = g << 2;
            b = b << 3;
            hWindow->attr.ColorKeyLower = (r << 16) | (g << 8) | b;
            break;
        }
        case NvColorFormat_VYUY:
            // ignore
        case NvColorFormat_Y8:
            // ignore
        default:
            break;
        }

        hWindow->bColorKeyDirty = NV_FALSE;
    }

    hal = &hWindow->hController->hal;
    hal->HwSetColorKey( hWindow->hController->Index,
         hWindow->attr.Number, hWindow->attr.ColorKeyUpper,
         hWindow->attr.ColorKeyLower );

    #undef PIXEL_COMPONENTS_565
    #undef PIXEL_COMPONENTS_A8B8G8R8
}

static NvError
NvDdkDispPrivSetControllerAttributes(
    NvDdkDispControllerHandle hController,
    NvDdkDispControllerAttribute *pAttributes,
    const NvU32 *pValues,
    NvU32 nAttr,
    NvU32 flags,
    NvBool verif )
{
    NvU32 i;
    NvU32 val;
    NvBool commit;
    NvDdkDispHal *hal;
    NvDdkDispCapabilities *caps = 0;
    NvBool bSD = NV_FALSE;
    NvBool update_sd = NV_FALSE;

    caps = hController->hDisp->caps;

    hal = &hController->hal;
    commit = ( !verif && hController->state == ControllerState_Active );
    bSD = NVDDK_DISP_CAP_IS_SET( caps, NvDdkDispCapBit_SmartDimmer );

    for( i = 0; i < nAttr; i++ )
    {
        val = *pValues;
        switch( *pAttributes ) {
        case NvDdkDispControllerAttribute_BackgroundColor:
            if( !verif )
            {
                hController->attr.BackgroundColor = val;
            }

            if( commit )
            {
                hal->HwSetBackgroundColor( hController->Index, val );
            }
            break;
        case NvDdkDispControllerAttribute_SmartDimmer_Enable:
            if( verif )
            {
                if( !bSD || val > 2 )
                    return NvError_BadParameter;
                return NvSuccess;
            }
            else
            {
                hController->attr.SmartDimmer_Enable = (NvU8)val;
                update_sd = NV_TRUE;
            }
            break;
        case NvDdkDispControllerAttribute_SmartDimmer_BinWidth:
            if( verif )
            {
                if( !bSD || val > 4 )
                {
                    return NvError_BadParameter;
                }
                return NvSuccess;
            }
            else
            {
                hController->attr.SmartDimmer_BinWidth = (NvU8)val;
                update_sd = NV_TRUE;
            }
            break;
        case NvDdkDispControllerAttribute_SmartDimmer_Aggressiveness:
            if( verif )
            {
                if( !bSD || val > 5 )
                {
                    return NvError_BadParameter;
                }
                return NvSuccess;
            }
            else
            {
                hController->attr.SmartDimmer_Aggressiveness = (NvU8)val;
                update_sd = NV_TRUE;
            }
            break;
        case NvDdkDispControllerAttribute_SmartDimmer_HwUpdateDelay:
            if( verif )
            {
                if( !bSD || val > 3 )
                {
                    return NvError_BadParameter;
                }
                return NvSuccess;
            }
            else
            {
                hController->attr.SmartDimmer_HwUpdateDelay = (NvU8)val;
                update_sd = NV_TRUE;
            }
            break;
        case NvDdkDispControllerAttribute_SmartDimmer_Use_VideoLuminance:
            if( verif )
            {
                if( !bSD )
                {
                    return NvError_BadParameter;
                }
                return NvSuccess;
            }
            else
            {
                hController->attr.SmartDimmer_VideoLuminance = (NvBool)val;
                update_sd = NV_TRUE;
            }
            break;
        case NvDdkDispControllerAttribute_SmartDimmer_Csc_Coeff_R:
            if( verif )
            {
                if( !bSD )
                {
                    return NvError_BadParameter;
                }
                return NvSuccess;
            }
            else
            {
                hController->attr.SmartDimmer_Csc_Coeff_R = (NvU16)val;
                update_sd = NV_TRUE;
            }
            break;
        case NvDdkDispControllerAttribute_SmartDimmer_Csc_Coeff_G:
            if( verif )
            {
                if( !bSD )
                {
                    return NvError_BadParameter;
                }
                return NvSuccess;
            }
            else
            {
                hController->attr.SmartDimmer_Csc_Coeff_G = (NvU16)val;
                update_sd = NV_TRUE;
            }
            break;
        case NvDdkDispControllerAttribute_SmartDimmer_Csc_Coeff_B:
            if( verif )
            {
                if( !bSD )
                {
                    return NvError_BadParameter;
                }
                return NvSuccess;
            }
            else
            {
                hController->attr.SmartDimmer_Csc_Coeff_B = (NvU16)val;
                update_sd = NV_TRUE;
            }
            break;
        case NvDdkDispControllerAttribute_SmartDimmer_Man_K_Enable:
            if( verif )
            {
                if( !bSD )
                {
                    return NvError_BadParameter;
                }
                return NvSuccess;
            }
            else
            {
                hController->attr.SmartDimmer_Man_K_Enable = (NvBool)val;
                update_sd = NV_TRUE;
            }
            break;
        case NvDdkDispControllerAttribute_SmartDimmer_Man_K_R:
            if( verif )
            {
                if( !bSD )
                {
                    return NvError_BadParameter;
                }
                return NvSuccess;
            }
            else
            {
                hController->attr.SmartDimmer_Man_K_R = (NvU16)val;
                update_sd = NV_TRUE;
            }
            break;
        case NvDdkDispControllerAttribute_SmartDimmer_Man_K_G:
            if( verif )
            {
                if( !bSD )
                {
                    return NvError_BadParameter;
                }
                return NvSuccess;
            }
            else
            {
                hController->attr.SmartDimmer_Man_K_G = (NvU16)val;
                update_sd = NV_TRUE;
            }
            break;
        case NvDdkDispControllerAttribute_SmartDimmer_Man_K_B:
            if( verif )
            {
                if( !bSD )
                {
                    return NvError_BadParameter;
                }
                return NvSuccess;
            }
            else
            {
                hController->attr.SmartDimmer_Man_K_B = (NvU16)val;
                update_sd = NV_TRUE;
            }
            break;
        case NvDdkDispControllerAttribute_SmartDimmer_FlickerTimeLimit:
            if( verif )
            {
                if( !bSD )
                {
                    return NvError_BadParameter;
                }
                return NvSuccess;
            }
            else
            {
                hController->attr.SmartDimmer_FlickerTimeLimit = (NvU8)val;
                update_sd = NV_TRUE;
            }
            break;
        case NvDdkDispControllerAttribute_SmartDimmer_FlickerThreshold:
            if( verif )
            {
                if( !bSD )
                {
                    return NvError_BadParameter;
                }
                return NvSuccess;
            }
            else
            {
                hController->attr.SmartDimmer_FlickerThreshold = (NvU8)val;
                update_sd = NV_TRUE;
            }
            break;
        case NvDdkDispControllerAttribute_SmartDimmer_BlStep:
            if( verif )
            {
                if( !bSD )
                {
                    return NvError_BadParameter;
                }
                return NvSuccess;
            }
            else
            {
                hController->attr.SmartDimmer_BlStep = (NvU16)val;
                update_sd = NV_TRUE;
            }
            break;
        case NvDdkDispControllerAttribute_SmartDimmer_BlTimeConstant:
            if( verif )
            {
                if( !bSD )
                {
                    return NvError_BadParameter;
                }
                return NvSuccess;
            }
            else
            {
                hController->attr.SmartDimmer_BlTimeConstant = (NvU16)val;
                update_sd = NV_TRUE;
            }
            break;
        case NvDdkDispControllerAttribute_Supports_SmartDimmer:
            // read-only, fall through
        default:
            return NvError_BadParameter;
        }

        pAttributes++;
        pValues++;
    }


    if( update_sd && commit && bSD )
    {
        NvBool manual;
        NvDdkSmartDimmerSettings sdSettings;
        sdSettings.Enable = hController->attr.SmartDimmer_Enable;
        sdSettings.BinWidth = hController->attr.SmartDimmer_BinWidth;
        sdSettings.Aggressiveness = hController->attr.SmartDimmer_Aggressiveness;
        sdSettings.HwUpdateDelay = hController->attr.SmartDimmer_HwUpdateDelay;
        sdSettings.bEnableVidLum = hController->attr.SmartDimmer_VideoLuminance;
        sdSettings.Coeff_R = hController->attr.SmartDimmer_Csc_Coeff_R;
        sdSettings.Coeff_G = hController->attr.SmartDimmer_Csc_Coeff_G;
        sdSettings.Coeff_B = hController->attr.SmartDimmer_Csc_Coeff_B;
        sdSettings.bEnableManK = hController->attr.SmartDimmer_Man_K_Enable;
        sdSettings.ManK_R = hController->attr.SmartDimmer_Man_K_R;
        sdSettings.ManK_G = hController->attr.SmartDimmer_Man_K_G;
        sdSettings.ManK_B = hController->attr.SmartDimmer_Man_K_B;
        sdSettings.FlickerTimeLimit = hController->attr.SmartDimmer_FlickerTimeLimit;
        sdSettings.FlickerThreshold = hController->attr.SmartDimmer_FlickerThreshold;
        sdSettings.BlStep = hController->attr.SmartDimmer_BlStep;
        sdSettings.BlTimeConstant = hController->attr.SmartDimmer_BlTimeConstant;

        hal->HwSmartDimmerConfig( hController->Index,
            &sdSettings, &manual );

        /* the smart dimmer thread will go to sleep if the enable bit is
         * disabled or if the controller becomes non-active.
         */

        if( manual && hController->attr.SmartDimmer_Enable )
        {
            if( hController->smartdimmer_thread == 0 )
            {
                NvBool b;
                b = NvDdkDispPrivCreateSmartDimmerThread( hController );
                if( !b )
                {
                    // Zero out the struct to reset everything.
                    NvOsMemset(&sdSettings, 0, sizeof(sdSettings));
                    hal->HwSmartDimmerConfig( hController->Index,
                        &sdSettings, NULL );
                    return NvError_InvalidState;
                }
            }
            else
            {
                NvOsSemaphoreSignal( hController->smartdimmer_thread->sem );
            }
        }
    }

    return NvSuccess;
}

NvError
NvDdkDispSetControllerAttributes(
    NvDdkDispControllerHandle hController,
    NvDdkDispControllerAttribute *pAttributes,
    const NvU32 *pValues,
    NvU32 nAttr,
    NvU32 flags)
{
    NvError err;
    ControllerAttr attr = {0};
    NvDdkDispHal *hal;
    NvBool b;

    NV_ASSERT( hController );
    NV_ASSERT( pAttributes );
    NV_ASSERT( pValues );
    NV_ASSERT( nAttr );
    NV_ASSERT( flags == 0 );


    /* verify attributes */
    err = NvDdkDispPrivSetControllerAttributes( hController, pAttributes,
        pValues, nAttr, flags, NV_TRUE );
    if( err != NvSuccess )
    {
        return err;
    }

    NvOsMutexLock( hController->mutex );

    hal = &hController->hal;

    /* keep a copy of the attributes in case the hardware commit fails.
     * roll back the changes.
     */
    if( hController->state == ControllerState_Active )
    {
        attr = hController->attr;

        hal->HwBegin( hController->Index );
    }

    /* apply attributes */
    NV_ASSERT_SUCCESS(
        NvDdkDispPrivSetControllerAttributes( hController, pAttributes,
            pValues, nAttr, flags, NV_FALSE )
    );

    /* commit to hardware */
    if( hController->state == ControllerState_Active )
    {
        b = hal->HwEnd( hController->Index, flags );
        if( !b )
        {
            /* roll back attributes */
            hController->attr = attr;
            goto fail;
        }
    }

    err = NvSuccess;
    goto clean;

fail:
    err = NvError_BadParameter;

clean:
    NvOsMutexUnlock( hController->mutex );
    return err;
}

NvError
NvDdkDispSetControllerAttribute(
    NvDdkDispControllerHandle hController,
    NvDdkDispControllerAttribute attribute,
    NvU32 value,
    NvU32 flags)
{
    return NvDdkDispSetControllerAttributes( hController, &attribute, &value,
        1, flags );
}

NvError
NvDdkDispGetControllerAttribute(
    NvDdkDispControllerHandle hController,
    NvDdkDispControllerAttribute attribute,
    NvU32 *pValue )
{
    NvError err;

    NV_ASSERT( hController );
    NV_ASSERT( attribute );
    NV_ASSERT( pValue );

    err = NvSuccess;
    NvOsMutexLock( hController->mutex );

    switch( attribute ) {
    case NvDdkDispControllerAttribute_BackgroundColor:
        *pValue = hController->attr.BackgroundColor;
        break;
    case NvDdkDispControllerAttribute_Supports_SmartDimmer:
    {
        NvDdkDispCapabilities *caps;
        NvBool bSD = NV_FALSE;
        caps = hController->hDisp->caps;
        bSD = NVDDK_DISP_CAP_IS_SET( caps, NvDdkDispCapBit_SmartDimmer );
        *pValue = bSD;
        break;
    }
    case NvDdkDispControllerAttribute_SmartDimmer_Enable:
        *pValue = hController->attr.SmartDimmer_Enable;
        break;
    case NvDdkDispControllerAttribute_SmartDimmer_Aggressiveness:
        *pValue = hController->attr.SmartDimmer_Aggressiveness;
        break;
    case NvDdkDispControllerAttribute_SmartDimmer_Use_VideoLuminance:
        *pValue = hController->attr.SmartDimmer_VideoLuminance;
        break;
    case NvDdkDispControllerAttribute_SmartDimmer_Csc_Coeff_R:
        *pValue = hController->attr.SmartDimmer_Csc_Coeff_R;
        break;
    case NvDdkDispControllerAttribute_SmartDimmer_Csc_Coeff_G:
        *pValue = hController->attr.SmartDimmer_Csc_Coeff_G;
        break;
    case NvDdkDispControllerAttribute_SmartDimmer_Csc_Coeff_B:
        *pValue = hController->attr.SmartDimmer_Csc_Coeff_B;
        break;
    case NvDdkDispControllerAttribute_SmartDimmer_Man_K_Enable:
        *pValue = hController->attr.SmartDimmer_Man_K_Enable;
        break;
    case NvDdkDispControllerAttribute_SmartDimmer_Man_K_R:
        *pValue = hController->attr.SmartDimmer_Man_K_R;
        break;
    case NvDdkDispControllerAttribute_SmartDimmer_Man_K_G:
        *pValue = hController->attr.SmartDimmer_Man_K_G;
        break;
    case NvDdkDispControllerAttribute_SmartDimmer_Man_K_B:
        *pValue = hController->attr.SmartDimmer_Man_K_B;
        break;
    default:
        err = NvError_BadParameter;
        break;
    }

    NvOsMutexUnlock( hController->mutex );

    return err;
}

static NvError
NvDdkDispPrivSetDisplayAttributes(
    NvDdkDispDisplayHandle hDisplay,
    NvDdkDispDisplayAttribute *pAttributes,
    const NvU32 *pValues,
    NvU32 nAttr,
    NvU32 flags,
    NvBool verif )
{
    NvU32 i;
    NvU32 val;
    NvBool commit;
    NvDdkDispHal *hal = 0;
    NvDdkDispCapabilities *caps = 0;
    NvBool update_backlight = NV_FALSE;
    NvBool update_tv_attr = NV_FALSE;
    NvBool update_tv_pos = NV_FALSE;
    NvBool update_tv_type = NV_FALSE;
    NvBool update_correction = NV_FALSE;

    if( hDisplay->CurrentController )
    {
        caps = hDisplay->CurrentController->hDisp->caps;
    }

    commit = ( !verif && hDisplay->CurrentController &&
        hDisplay->CurrentController->state == ControllerState_Active );

    if( commit )
    {
        hal = &hDisplay->CurrentController->hal;
    }

    for( i = 0; i < nAttr; i++ )
    {
        val = *pValues;

        switch( *pAttributes ) {
        case NvDdkDispDisplayAttribute_Type:
        case NvDdkDispDisplayAttribute_Usage:
        case NvDdkDispDisplayAttribute_MaxHorizontalResolution:
        case NvDdkDispDisplayAttribute_MaxVerticalResolution:
        case NvDdkDispDisplayAttribute_ColorPrecision:
        case NvDdkDispDisplayAttribute_SupportsHdtvAnalog:
            return NvError_ReadOnlyAttribute;

        case NvDdkDispDisplayAttribute_Brightness:
            if( !verif )
            {
                hDisplay->attr.Brightness = val;
                update_correction = NV_TRUE;
            }
            break;
        case NvDdkDispDisplayAttribute_Gamma:
            if( !verif )
            {
                hDisplay->attr.GammaRed = val;
                hDisplay->attr.GammaGreen = val;
                hDisplay->attr.GammaBlue = val;
                update_correction = NV_TRUE;
            }
            break;
        case NvDdkDispDisplayAttribute_GammaRed:
            if( !verif )
            {
                hDisplay->attr.GammaRed = val;
                update_correction = NV_TRUE;
            }
            break;
        case NvDdkDispDisplayAttribute_GammaGreen:
            if( !verif )
            {
                hDisplay->attr.GammaGreen = val;
                update_correction = NV_TRUE;
            }
            break;
        case NvDdkDispDisplayAttribute_GammaBlue:
            if( !verif )
            {
                hDisplay->attr.GammaBlue = val;
                update_correction = NV_TRUE;
            }
            break;
        case NvDdkDispDisplayAttribute_Contrast:
            if( !verif )
            {
                hDisplay->attr.Contrast = val;
                update_correction = NV_TRUE;
            }
            break;

        case NvDdkDispDisplayAttribute_Backlight:
        {
            NvDdkDispBacklightControl c = (NvDdkDispBacklightControl)val;
            if( verif )
            {
                if( c != NvDdkDispBacklightControl_Off &&
                    c != NvDdkDispBacklightControl_On &&
                    c != NvDdkDispBacklightControl_OnStrobe )
                {
                    return NvError_BadParameter;
                }
            }
            else
            {
                if( hDisplay->attr.Backlight !=
                    (NvDdkDispBacklightControl)val )
                {
                    update_backlight = NV_TRUE;
                }

                hDisplay->attr.Backlight = (NvDdkDispBacklightControl)val;

                if( (c == NvDdkDispBacklightControl_On) &&
                    (hDisplay->attr.InBacklightIntensity == 0) )
                {
                    hDisplay->attr.InBacklightIntensity = 255;
                }
            }

            break;
        }
        case NvDdkDispDisplayAttribute_BacklightIntensity:
            if( verif )
            {
                if( val > 255 )
                {
                    return NvError_BadParameter;
                }
            }
            else
            {
                hDisplay->attr.InBacklightIntensity = (NvU8)val;
                if( val == 0 )
                {
                    hDisplay->attr.Backlight = NvDdkDispBacklightControl_Off;
                }
                else
                {
                    if (hDisplay->attr.Backlight == NvDdkDispBacklightControl_Off)
                        hDisplay->attr.Backlight = NvDdkDispBacklightControl_On;
                }
                update_backlight = NV_TRUE;
            }

            break;
        case NvDdkDispDisplayAttribute_BacklightTimeout:
            if( !verif )
            {
                hDisplay->attr.BacklightTimeout = val;
                update_backlight = NV_TRUE;
            }

            break;
        case NvDdkDispDisplayAttribute_AutoBacklight:
            if (!verif)
            {
                hDisplay->attr.AutoBacklight = (NvBool)val;
            }
            if (commit)
            {
                NvOdmDispDeviceHandle hPanel;
                hPanel = hDisplay->panel->hPanel;
                if( hPanel )
                {
                    NvU8 cfg;
                    cfg = (NvU8)hDisplay->attr.AutoBacklight;
                    NvOdmDispSetSpecialFunction(hPanel,
                        NvOdmDispSpecialFunction_AutoBacklight, &cfg);
                }
            }
            break;
        case NvDdkDispDisplayAttribute_AudioFrequency:
            if( verif )
            {
                switch( (NvDdkDispAudioFrequency)val ) {
                case NvDdkDispAudioFrequency_00000hz:
                case NvDdkDispAudioFrequency_32000hz:
                case NvDdkDispAudioFrequency_44100hz:
                case NvDdkDispAudioFrequency_48000hz:
                    break;
                default:
                    return NvError_BadParameter;
                }
            }
            else
            {
                hDisplay->attr.AudioFrequency = (NvDdkDispAudioFrequency)val;
            }

            if( commit )
            {
                NvDdkDispPanel *p;
                p = hDisplay->panel;
                if( p && p->builtin && p->builtin->AudioFrequency )
                {
                    p->builtin->AudioFrequency(
                        hDisplay->CurrentController->Index,
                        hDisplay->attr.AudioFrequency );
                }
            }

            break;
        case NvDdkDispDisplayAttribute_PartialModeLineOffset:
            if( !verif )
            {
                hDisplay->attr.PartialModeLineOffset = val;
            }

            if( commit )
            {
                NvOdmDispDeviceHandle hPanel;
                hPanel = hDisplay->panel->hPanel;
                if( hPanel )
                {
                    NvOdmDispPartialModeConfig cfg;
                    cfg.LineOffset = hDisplay->attr.PartialModeLineOffset;

                    NvOdmDispSetSpecialFunction( hPanel,
                        NvOdmDispSpecialFunction_PartialMode, &cfg );
                }
            }
            break;
        case NvDdkDispDisplayAttribute_TV_Type:
            if( verif )
            {
                switch( (NvDdkDispTvType)val ) {
                case NvDdkDispTvType_Ntsc:
                case NvDdkDispTvType_Pal:
                    // ok
                    break;
                case NvDdkDispTvType_Hdtv:
                    if( !caps )
                    {
                        return NvError_BadParameter;
                    }

                    if( NVDDK_DISP_CAP_IS_SET( caps,
                            NvDdkDispCapBit_Hdtv_Analog ) )
                    {
                        // ok
                        break;
                    }
                default:
                    // not ok
                    return NvError_BadParameter;
                }
            }
            else
            {
                hDisplay->attr.TV_Type = (NvDdkDispTvType)val;
                update_tv_type = NV_TRUE;
            }
            break;
        case NvDdkDispDisplayAttribute_TV_OutputFormat:
            if( verif )
            {
                switch( (NvDdkDispTvOutput)val ) {
                case NvDdkDispTvOutput_Composite:
                case NvDdkDispTvOutput_Component:
                case NvDdkDispTvOutput_SVideo:
                    // ok
                    break;
                default:
                    return NvError_BadParameter;
                }
            }
            else
            {
                hDisplay->attr.TV_OutputFormat = (NvDdkDispTvOutput)val;
                update_tv_attr = NV_TRUE;
            }
            break;
        case NvDdkDispDisplayAttribute_TV_Overscan:
            if( !verif )
            {
                hDisplay->attr.TV_Overscan = val;
                update_tv_attr = NV_TRUE;
            }
            break;
        case NvDdkDispDisplayAttribute_TV_OverscanY:
            if( !verif )
            {
                hDisplay->attr.TV_OverscanY = val;
                update_tv_attr = NV_TRUE;
            }
            break;
        case NvDdkDispDisplayAttribute_TV_OverscanCr:
            if( !verif )
            {
                hDisplay->attr.TV_OverscanCr = val;
                update_tv_attr = NV_TRUE;
            }
            break;
        case NvDdkDispDisplayAttribute_TV_OverscanCb:
            if( !verif )
            {
                hDisplay->attr.TV_OverscanCb = val;
                update_tv_attr = NV_TRUE;
            }
            break;
        case NvDdkDispDisplayAttribute_TV_HorizontalPosition:
            if( !verif )
            {
                hDisplay->attr.TV_HorizontalPosition = val;
                update_tv_pos = NV_TRUE;
            }
            break;
        case NvDdkDispDisplayAttribute_TV_VerticalPosition:
            if( !verif )
            {
                hDisplay->attr.TV_VerticalPosition = val;
                update_tv_pos = NV_TRUE;
            }
            break;
        case NvDdkDispDisplayAttribute_TV_DAC_Amplitude:
            if( !verif )
            {
                hDisplay->attr.TV_DAC_Amplitude = ((NvU8)val & 0xFF);
                update_tv_attr = NV_TRUE;
            }
            break;
        case NvDdkDispDisplayAttribute_TV_FilterMode:
            if( !verif )
            {
                hDisplay->attr.TV_FilterMode = (NvDdkDispTvFilterMode)val;
                update_tv_attr = NV_TRUE;
            }
            break;
        case NvDdkDispDisplayAttribute_TV_ScreenFormat:
            if( !verif )
            {
                hDisplay->attr.TV_ScreenFormat = (NvDdkDispTvScreenFormat)val;
                update_tv_attr = NV_TRUE;
            }
            break;
        default:
            return NvError_BadParameter;
        }

        pAttributes++;
        pValues++;
    }

    if( update_backlight && commit )
    {
        NvU8 intensity = 0;

        if( hDisplay->attr.Backlight == NvDdkDispBacklightControl_OnStrobe )
        {
            hDisplay->CurrentController->timeout_thread->timeout =
                hDisplay->attr.BacklightTimeout;
        }
        else
        {
            hDisplay->CurrentController->timeout_thread->timeout =
                NV_WAIT_INFINITE;
        }

        if( hDisplay->attr.Backlight == NvDdkDispBacklightControl_On ||
            hDisplay->attr.Backlight == NvDdkDispBacklightControl_OnStrobe   )
        {
            intensity = hDisplay->attr.InBacklightIntensity;
        }

        NvOsSemaphoreSignal(
            hDisplay->CurrentController->timeout_thread->sem );

        hal->HwSetBacklight( hDisplay->CurrentController->Index, intensity );
        hDisplay->attr.OutBacklightIntensity = intensity;
        if( hDisplay->panel->hPanel )
        {
            NvOdmDispSetBacklight( hDisplay->panel->hPanel, intensity );
        }
    }
    if( update_tv_type )
    {
        NvDdkDispPanel *p;

        /* this changes the type on all controllers, just pick the hal
         * for the first one.
         */
        if( !hal )
        {
            NvDdkDispControllerHandle hController;
            hController = &hDisplay->hDisp->controllers[0];
            hal = &hController->hal;
        }

        hal->TvHal->HwTvType( hDisplay->attr.TV_Type );

        /* need to refresh the mode list */
        p = hDisplay->panel;
        NV_ASSERT( p && p->builtin );
        p->nModes = 0;
        p->builtin->ListModes( &p->nModes, 0 );
        p->nModes = NV_MIN( NVDDK_DISP_MAX_MODES, p->nModes );
        p->builtin->ListModes( &p->nModes, p->modes );
    }
    if( update_tv_pos && commit )
    {
        if( hDisplay->attr.Type == NvDdkDispDisplayType_TV )
        {
            hal->TvHal->HwTvPosition( hDisplay->CurrentController->Index,
                hDisplay->attr.TV_HorizontalPosition,
                hDisplay->attr.TV_VerticalPosition );
        }
    }
    if( update_tv_attr && commit )
    {
        if( hDisplay->attr.Type == NvDdkDispDisplayType_TV )
        {
            DisplayAttr *attr;
            attr = &hDisplay->attr;
            hal->TvHal->HwTvAttrs( hDisplay->CurrentController->Index,
                attr->TV_Overscan, attr->TV_OverscanY, attr->TV_OverscanCb,
                attr->TV_OverscanCr, attr->TV_OutputFormat,
                attr->TV_DAC_Amplitude, attr->TV_FilterMode,
                attr->TV_ScreenFormat);
        }
    }
    if( update_correction && commit )
    {
        hal->HwSetColorCorrection( hDisplay->CurrentController->Index,
            hDisplay->attr.Brightness, hDisplay->attr.GammaRed,
            hDisplay->attr.GammaGreen, hDisplay->attr.GammaBlue,
            hDisplay->attr.ScaleRed, hDisplay->attr.ScaleGreen,
            hDisplay->attr.ScaleBlue, hDisplay->attr.Contrast );
    }

    return NvSuccess;
}

NvError
NvDdkDispSetDisplayAttributes(
    NvDdkDispDisplayHandle hDisplay,
    NvDdkDispDisplayAttribute *pAttributes,
    const NvU32 *pValues,
    NvU32 nAttr,
    NvU32 flags)
{
    NvError err;
    DisplayAttr attr;
    NvDdkDispHal *hal = 0;
    NvBool commit;
    NvBool b;

    NV_ASSERT( hDisplay );
    NV_ASSERT( pAttributes );
    NV_ASSERT( pValues );
    NV_ASSERT( nAttr );
    NV_ASSERT( flags == 0 ||
        !( (flags & ~NVDDK_DISP_WAIT )));

    /* verify */
    err = NvDdkDispPrivSetDisplayAttributes( hDisplay, pAttributes,
        pValues, nAttr, flags, NV_TRUE );
    if( err != NvSuccess )
    {
        return err;
    }

    NvOsMutexLock( hDisplay->mutex );

    commit = ( hDisplay->CurrentController &&
        hDisplay->CurrentController->state == ControllerState_Active );

    if( commit )
    {
        /* keep attribute copy for rollback */
        attr = hDisplay->attr;

        hal = &hDisplay->CurrentController->hal;
        hal->HwBegin( hDisplay->CurrentController->Index );
    }

    /* apply */
    NV_ASSERT_SUCCESS(
        NvDdkDispPrivSetDisplayAttributes( hDisplay, pAttributes,
            pValues, nAttr, flags, NV_FALSE )
    );

    /* commit to hardware */
    if( commit )
    {
        b = hal->HwEnd( hDisplay->CurrentController->Index, flags );
        if( !b )
        {
            /* roll back attributes */
            hDisplay->attr = attr;
            goto fail;
        }
    }

    err = NvSuccess;
    goto clean;

fail:
    err = NvError_BadParameter;

clean:
    NvOsMutexUnlock( hDisplay->mutex );

    return err;
}

NvError
NvDdkDispSetDisplayAttribute(
    NvDdkDispDisplayHandle hDisplay,
    NvDdkDispDisplayAttribute attribute,
    NvU32 value,
    NvU32 flags)
{
    return NvDdkDispSetDisplayAttributes( hDisplay, &attribute, &value, 1,
        flags );
}

NvError
NvDdkDispGetDisplayAttribute(
    NvDdkDispDisplayHandle hDisplay,
    NvDdkDispDisplayAttribute attribute,
    NvU32 *pValue )
{
    NvError err = NvSuccess;
    NvDdkDispCapabilities *caps = 0;

    NV_ASSERT( hDisplay );
    NV_ASSERT( hDisplay->hDisp );
    NV_ASSERT( pValue );

    caps = hDisplay->hDisp->caps;
    NV_ASSERT( caps );

    NvOsMutexLock( hDisplay->mutex );

    switch( attribute ) {
    case NvDdkDispDisplayAttribute_Type:
        *pValue = hDisplay->attr.Type;
        break;
    case NvDdkDispDisplayAttribute_Usage:
        *pValue = hDisplay->attr.Usage;
        break;
    case NvDdkDispDisplayAttribute_MaxHorizontalResolution:
        *pValue = hDisplay->attr.MaxHorizontalResolution;
        break;
    case NvDdkDispDisplayAttribute_MaxVerticalResolution:
        *pValue = hDisplay->attr.MaxVerticalResolution;
        break;
    case NvDdkDispDisplayAttribute_ColorPrecision:
        *pValue = hDisplay->attr.ColorPrecision;
        break;
    case NvDdkDispDisplayAttribute_SupportsHdtvAnalog:
        *pValue = (NvU32) NVDDK_DISP_CAP_IS_SET( caps,
            NvDdkDispCapBit_Hdtv_Analog ) ? 1:0;
        break;
    case NvDdkDispDisplayAttribute_Backlight:
        *pValue = hDisplay->attr.Backlight;
        break;
    case NvDdkDispDisplayAttribute_BacklightIntensity:
        *pValue = hDisplay->attr.InBacklightIntensity;
        break;
    case NvDdkDispDisplayAttribute_BacklightTimeout:
        *pValue = hDisplay->attr.BacklightTimeout;
        break;
    case NvDdkDispDisplayAttribute_AutoBacklight:
        *pValue = (NvU32)hDisplay->attr.AutoBacklight;
        break;
    case NvDdkDispDisplayAttribute_AudioFrequency:
        *pValue = hDisplay->attr.AudioFrequency;
        break;
    case NvDdkDispDisplayAttribute_Brightness:
        *pValue = hDisplay->attr.Brightness;
        break;
    case NvDdkDispDisplayAttribute_Gamma:
        /* this assumes that the attribute was set with Gamma, not
         * with GammaRed, Green, or Blue.
         */
        *pValue = hDisplay->attr.GammaRed;
        break;
    case NvDdkDispDisplayAttribute_GammaRed:
        *pValue = hDisplay->attr.GammaRed;
        break;
    case NvDdkDispDisplayAttribute_GammaGreen:
        *pValue = hDisplay->attr.GammaGreen;
        break;
    case NvDdkDispDisplayAttribute_GammaBlue:
        *pValue = hDisplay->attr.GammaBlue;
        break;
    case NvDdkDispDisplayAttribute_Contrast:
        *pValue = hDisplay->attr.Contrast;
        break;
    case NvDdkDispDisplayAttribute_PartialModeLineOffset:
        *pValue = hDisplay->attr.PartialModeLineOffset;
        break;
    case NvDdkDispDisplayAttribute_TV_Type:
        *pValue = hDisplay->attr.TV_Type;
        break;
    case NvDdkDispDisplayAttribute_TV_OutputFormat:
        *pValue = hDisplay->attr.TV_OutputFormat;
        break;
    case NvDdkDispDisplayAttribute_TV_Overscan:
        *pValue = hDisplay->attr.TV_Overscan;
        break;
    case NvDdkDispDisplayAttribute_TV_OverscanY:
        *pValue = hDisplay->attr.TV_OverscanY;
        break;
    case NvDdkDispDisplayAttribute_TV_OverscanCr:
        *pValue = hDisplay->attr.TV_OverscanCr;
        break;
    case NvDdkDispDisplayAttribute_TV_OverscanCb:
        *pValue = hDisplay->attr.TV_OverscanCb;
        break;
    case NvDdkDispDisplayAttribute_TV_HorizontalPosition:
        *pValue = hDisplay->attr.TV_HorizontalPosition;
        break;
    case NvDdkDispDisplayAttribute_TV_VerticalPosition:
        *pValue = hDisplay->attr.TV_VerticalPosition;
        break;
    case NvDdkDispDisplayAttribute_TV_DAC_Amplitude:
        *pValue = hDisplay->attr.TV_DAC_Amplitude;
        break;
    case NvDdkDispDisplayAttribute_TV_FilterMode:
        *pValue = hDisplay->attr.TV_FilterMode;
        break;
    case NvDdkDispDisplayAttribute_TV_ScreenFormat:
        *pValue = hDisplay->attr.TV_ScreenFormat;
        break;
    case NvDdkDispDisplayAttribute_Dsi_Mode:
        *pValue = hDisplay->attr.mode;
    default:
        err = NvError_BadParameter;
        break;
    }

    NvOsMutexUnlock( hDisplay->mutex );
    return err;
}

static NvError
NvDdkDispPrivSetWindowAttributes(
    NvDdkDispWindowHandle hWindow,
    NvDdkDispWindowAttribute *pAttributes,
    const NvU32 *pValues,
    NvU32 nAttr,
    NvU32 flags,
    NvBool verif,
    NvBool commit )
{
    NvU32 i;
    NvU32 val;
    NvBool update_position = NV_FALSE;
    NvBool update_colorkey = NV_FALSE;
    NvBool update_csc = NV_FALSE;
    NvDdkDispHal *hal = 0;
    NvDdkDispCapabilities *caps;
    NvDdkDispWindowCap *wc;

    caps = hWindow->hController->hDisp->caps;

    if( commit )
    {
        hal = &hWindow->hController->hal;
    }

    // FIXME: bug 288826 - initial dda for overlay

    for( i = 0; i < nAttr; i++ )
    {
        val = *pValues;

        switch( *pAttributes ) {
        case NvDdkDispWindowAttribute_Number:
        case NvDdkDispWindowAttribute_Usage:
        case NvDdkDispWindowAttribute_SupportsColorSpaceConversion:
        case NvDdkDispWindowAttribute_SupportsHorizontalFiltering:
        case NvDdkDispWindowAttribute_SupportsVerticalFiltering:
        case NvDdkDispWindowAttribute_SupportsPremultipliedAlpha:
            return NvError_ReadOnlyAttribute;

        case NvDdkDispWindowAttribute_Depth:
            if( !verif )
            {
                hWindow->attr.Depth = val;
            }

            if( commit )
            {
                hal->HwSetDepth( hWindow->hController->Index,
                    hWindow->attr.Number, hWindow->attr.Depth );
            }
            break;
        case NvDdkDispWindowAttribute_Rotation:
            if( !verif )
            {
                hWindow->attr.Rotation = val;
                update_position = NV_TRUE;
            }

            break;
        case NvDdkDispWindowAttribute_Mirror:
            if( verif )
            {
                switch( (NvDdkDispMirror)val ) {
                case NvDdkDispMirror_None:
                case NvDdkDispMirror_Horizontal:
                case NvDdkDispMirror_Vertical:
                case NvDdkDispMirror_Both:
                    // ok
                    break;
                default:
                    return NvError_BadParameter;
                }
            }
            else
            {
                hWindow->attr.Mirror = (NvDdkDispMirror)val;
                update_position = NV_TRUE;
            }

            break;
        case NvDdkDispWindowAttribute_SourceRect_Left:
            if( !verif )
            {
                hWindow->attr.SourceRect.left = val;
                update_position = NV_TRUE;
            }
            break;
        case NvDdkDispWindowAttribute_SourceRect_Top:
            if( !verif )
            {
                hWindow->attr.SourceRect.top = val;
                update_position = NV_TRUE;
            }
            break;
        case NvDdkDispWindowAttribute_SourceRect_Right:
            if( !verif )
            {
                hWindow->attr.SourceRect.right = val;
                update_position = NV_TRUE;
            }
            break;
        case NvDdkDispWindowAttribute_SourceRect_Bottom:
            if( !verif )
            {
                hWindow->attr.SourceRect.bottom = val;
                update_position = NV_TRUE;
            }
            break;
        case NvDdkDispWindowAttribute_DestRect_Left:
            if( !verif )
            {
                hWindow->attr.DestRect.left = val;
                update_position = NV_TRUE;
            }
            break;
        case NvDdkDispWindowAttribute_DestRect_Top:
            if( !verif )
            {
                hWindow->attr.DestRect.top = val;
                update_position = NV_TRUE;
            }
            break;
        case NvDdkDispWindowAttribute_DestRect_Right:
            if( !verif )
            {
                hWindow->attr.DestRect.right = val;
                update_position = NV_TRUE;
            }
            break;
        case NvDdkDispWindowAttribute_DestRect_Bottom:
            if( !verif )
            {
                hWindow->attr.DestRect.bottom = val;
                update_position = NV_TRUE;
            }
            break;
        case NvDdkDispWindowAttribute_Location_X:
            if( !verif )
            {
                hWindow->attr.Location.x = val;
                update_position = NV_TRUE;
            }
            break;
        case NvDdkDispWindowAttribute_Location_Y:
            if( !verif )
            {
                hWindow->attr.Location.y = val;
                update_position = NV_TRUE;
            }
            break;
        case NvDdkDispWindowAttribute_Filtering:
        case NvDdkDispWindowAttribute_Filtering_Horizontal:
        case NvDdkDispWindowAttribute_Filtering_Vertical:
            if( verif )
            {
                wc = &caps->WindowCaps[hWindow->attr.Number];
                /* allow filter disable regardless of filter cap. */
                if( *pAttributes ==
                      NvDdkDispWindowAttribute_Filtering_Horizontal &&
                    !wc->bHorizFilter && val )
                {
                    return NvError_BadParameter;
                }
                else if( *pAttributes ==
                      NvDdkDispWindowAttribute_Filtering_Vertical &&
                    !wc->bVertFilter && val )
                {
                    return NvError_BadParameter;
                }
                else if( !(wc->bHorizFilter || wc->bVertFilter) && val )
                {
                    return NvError_BadParameter;
                }
            }
            else
            {
                if( *pAttributes ==
                    NvDdkDispWindowAttribute_Filtering_Horizontal )
                {
                    hWindow->attr.HorizFiltering = (NvBool)val;
                }
                else if( *pAttributes ==
                    NvDdkDispWindowAttribute_Filtering_Vertical )
                {
                    hWindow->attr.VertFiltering = (NvBool)val;
                }
                else
                {
                    hWindow->attr.HorizFiltering = (NvBool)val;
                    hWindow->attr.VertFiltering = (NvBool)val;
                }
            }

            if( commit )
            {
                hal->HwSetFiltering( hWindow->hController->Index,
                    hWindow->attr.Number, hWindow->attr.HorizFiltering,
                    hWindow->attr.VertFiltering );
            }
            break;
        case NvDdkDispWindowAttribute_DigitalVibrance:
            if( !verif )
            {
                hWindow->attr.DigitalVibrance = val;
            }

            if( commit )
            {
                hal->HwSetVibrance( hWindow->hController->Index,
                    hWindow->attr.Number, hWindow->attr.DigitalVibrance );
            }
            break;
        case NvDdkDispWindowAttribute_ScaleNicely:
            if( !verif )
            {
                hWindow->attr.ScaleNicely = (NvBool)val;
            }

            if( commit )
            {
                hal->HwSetScaleNicely( hWindow->hController->Index,
                    hWindow->attr.Number, hWindow->attr.ScaleNicely );
            }
            break;
        case NvDdkDispWindowAttribute_BlendType:
            if( verif )
            {
                switch( (NvDdkDispBlendType)val ) {
                case NvDdkDispBlendType_None:
                case NvDdkDispBlendType_Alpha:
                case NvDdkDispBlendType_PerPixelAlpha:
                case NvDdkDispBlendType_ColorKey:
                    // ok
                    break;
                default:
                    return NvError_BadParameter;
                }
            }
            else
            {
                hWindow->attr.BlendType = (NvDdkDispBlendType)val;
            }

            if( commit )
            {
                hal->HwSetBlend( hWindow->hController->Index,
                    hWindow->attr.Number, hWindow->attr.BlendType );
            }
            break;
        case NvDdkDispWindowAttribute_AlphaOperation:
            if( verif )
            {
                switch( (NvDdkDispAlphaOperation)val ) {
                case NvDdkDispAlphaOperation_WeightedMean:
                case NvDdkDispAlphaOperation_SimpleSum:
                    // ok
                    break;
                case NvDdkDispAlphaOperation_PremultipliedWeight:
                    wc = &caps->WindowCaps[hWindow->attr.Number];
                    if ( !wc->bPremultipliedAlpha )
                        return NvError_NotSupported;
                    break;
                default:
                    return NvError_BadParameter;
                }
            }
            else
            {
                hWindow->attr.AlphaOperation = (NvDdkDispAlphaOperation)val;
            }

            if( commit )
            {
                hal->HwSetAlphaOp( hWindow->hController->Index,
                    hWindow->attr.Number, hWindow->attr.AlphaOperation,
                    hWindow->attr.AlphaDirection );
            }
            break;
        case NvDdkDispWindowAttribute_AlphaDirection:
            if( verif )
            {
                switch( (NvDdkDispAlphaBlendDirection)val ) {
                case NvDdkDispAlphaBlendDirection_Default:
                case NvDdkDispAlphaBlendDirection_Source:
                case NvDdkDispAlphaBlendDirection_Destination:
                    // ok
                    break;
                default:
                    return NvError_BadParameter;
                }
            }
            else
            {
                hWindow->attr.AlphaDirection =
                    (NvDdkDispAlphaBlendDirection)val;
            }

            if( commit )
            {
                hal->HwSetAlphaOp( hWindow->hController->Index,
                    hWindow->attr.Number, hWindow->attr.AlphaOperation,
                    hWindow->attr.AlphaDirection );
            }
            break;
        case NvDdkDispWindowAttribute_AlphaValue:
            if( !verif )
            {
                hWindow->attr.AlphaValue = (NvU8)val;
            }

            if( commit )
            {
                hal->HwSetAlpha( hWindow->hController->Index,
                    hWindow->attr.Number, hWindow->attr.AlphaValue );
            }
            break;
        case NvDdkDispWindowAttribute_ColorKey:
            if( !verif )
            {
                hWindow->attr.ColorKeyLower = val;
                hWindow->attr.ColorKeyUpper = val;
                update_colorkey = NV_TRUE;
            }
            break;
        case NvDdkDispWindowAttribute_ColorKeyLower:
            if( !verif )
            {
                hWindow->attr.ColorKeyLower = val;
                update_colorkey = NV_TRUE;
            }
            break;
        case NvDdkDispWindowAttribute_ColorKeyUpper:
            if( !verif )
            {
                hWindow->attr.ColorKeyUpper = val;
                update_colorkey = NV_TRUE;
            }
            break;
        case NvDdkDispWindowAttribute_ColorSpaceCoef_C11:
            if( !verif )
            {
                hWindow->attr.ColorSpaceCoef.C11 = val;
                update_csc = NV_TRUE;
            }
            break;
        case NvDdkDispWindowAttribute_ColorSpaceCoef_C12:
            if( !verif )
            {
                hWindow->attr.ColorSpaceCoef.C12 = val;
                update_csc = NV_TRUE;
            }
            break;
        case NvDdkDispWindowAttribute_ColorSpaceCoef_C13:
            if( !verif )
            {
                hWindow->attr.ColorSpaceCoef.C13 = val;
                update_csc = NV_TRUE;
            }
            break;
        case NvDdkDispWindowAttribute_ColorSpaceCoef_C22:
            if( !verif )
            {
                hWindow->attr.ColorSpaceCoef.C22 = val;
                update_csc = NV_TRUE;
            }
            break;
        case NvDdkDispWindowAttribute_ColorSpaceCoef_C23:
            if( !verif )
            {
                hWindow->attr.ColorSpaceCoef.C23 = val;
                update_csc = NV_TRUE;
            }
            break;
        case NvDdkDispWindowAttribute_ColorSpaceCoef_C32:
            if( !verif )
            {
                hWindow->attr.ColorSpaceCoef.C32 = val;
                update_csc = NV_TRUE;
            }
            break;
        case NvDdkDispWindowAttribute_ColorSpaceCoef_C33:
            if( !verif )
            {
                hWindow->attr.ColorSpaceCoef.C33 = val;
                update_csc = NV_TRUE;
            }
            break;
        case NvDdkDispWindowAttribute_ColorSpaceCoef_K:
            if( !verif )
            {
                hWindow->attr.ColorSpaceCoef.K = val;
                update_csc = NV_TRUE;
            }
            break;
        default:
            return NvError_BadParameter;
        }

        pAttributes++;
        pValues++;
    }

    if( verif )
    {
        // FIXME: check source and dest rect for sanity
    }

    if( update_colorkey )
    {
        hWindow->bColorKeyDirty = NV_TRUE;
    }

    if( commit )
    {
        if( update_position &&
            !NvDdkDispPrivIsSurfaceNull( &hWindow->surface[0] ) )
        {
            NvBool bogus;
            NvDdkDispPrivUpdateSurface( hWindow->hController,
                hWindow, hWindow, &bogus );
        }
        if( update_csc )
        {
            hal->HwSetCsc( hWindow->hController->Index,
                hWindow->attr.Number, &hWindow->attr.ColorSpaceCoef );
        }
        if( update_colorkey && hWindow->nSurfaces )
        {
            NvDdkDispPrivColorKey( hWindow );
        }
    }

    return NvSuccess;
}

NvError
NvDdkDispSetWindowAttributes(
    NvDdkDispWindowHandle hWindow,
    NvDdkDispWindowAttribute *pAttributes,
    const NvU32 *pValues,
    NvU32 nAttr,
    NvU32 flags)
{
    NvError err;
    WindowAttr attr;
    NvDdkDispHal *hal = 0;
    NvBool commit;
    NvBool b;

    NV_ASSERT( hWindow );
    NV_ASSERT( hWindow->hController );
    NV_ASSERT( pAttributes );
    NV_ASSERT( pValues );
    NV_ASSERT( nAttr );
    NV_ASSERT( flags == 0 ||
        !( (flags & ~NVDDK_DISP_WAIT
                  & ~NVDDK_DISP_DO_NOT_COMMIT )));

    /* verify */
    err = NvDdkDispPrivSetWindowAttributes( hWindow, pAttributes, pValues,
        nAttr, flags, NV_TRUE, NV_FALSE );
    if( err != NvSuccess )
    {
        return err;
    }

    NvOsMutexLock( hWindow->hController->mutex );

    if( flags & NVDDK_DISP_DO_NOT_COMMIT )
    {
        hWindow->bDirty = NV_TRUE;
    }

    commit = ( hWindow->nSurfaces != 0 &&
        hWindow->hController->state == ControllerState_Active &&
        !(flags & NVDDK_DISP_DO_NOT_COMMIT) );
    if( commit )
    {
        /* keep a copy of the attributes for rollback on failure */
        attr = hWindow->attr;

        hal = &hWindow->hController->hal;
        hal->HwBegin( hWindow->hController->Index );
    }

    /* apply */
    NV_ASSERT_SUCCESS(
        NvDdkDispPrivSetWindowAttributes( hWindow, pAttributes, pValues,
            nAttr, flags, NV_FALSE, commit )
    );

    /* commit to hardware */
    if( commit )
    {
        if( hWindow->bDirty )
        {
            NvDdkDispPrivCommitWindow( hWindow );
        }

        b = hal->HwEnd( hWindow->hController->Index, flags );
        if( !b )
        {
            /* roll back changes */
            hWindow->attr = attr;
            goto fail;
        }
    }

    err = NvSuccess;
    goto clean;

fail:
    err = NvError_BadParameter;

clean:
    NvOsMutexUnlock( hWindow->hController->mutex );
    return err;
}

NvError
NvDdkDispSetWindowAttribute(
    NvDdkDispWindowHandle hWindow,
    NvDdkDispWindowAttribute attribute,
    NvU32 value,
    NvU32 flags)
{
    return NvDdkDispSetWindowAttributes( hWindow, &attribute, &value, 1,
        flags );
}

NvError
NvDdkDispGetWindowAttribute(
    NvDdkDispWindowHandle hWindow,
    NvDdkDispWindowAttribute attribute,
    NvU32 *pValue )
{
    NvError err = NvSuccess;
    NvDdkDispCapabilities *caps = 0;
    NvDdkDispWindowCap *wincap = 0;

    NV_ASSERT( hWindow );
    NV_ASSERT( hWindow->hController );
    NV_ASSERT( pValue );

    caps = hWindow->hController->hDisp->caps;
    NV_ASSERT( caps );
    NV_ASSERT( hWindow->attr.Number < caps->NumWindows );

    wincap = &caps->WindowCaps[hWindow->attr.Number];
    NV_ASSERT( wincap );

    NvOsMutexLock( hWindow->hController->mutex );

    switch( attribute ) {
    case NvDdkDispWindowAttribute_Number:
        *pValue = hWindow->attr.Number;
        break;
    case NvDdkDispWindowAttribute_Usage:
        *pValue = hWindow->attr.Usage;
        break;
    case NvDdkDispWindowAttribute_SupportsColorSpaceConversion:
        *pValue = (NvU32)wincap->bCsc;
        break;
    case NvDdkDispWindowAttribute_SupportsHorizontalFiltering:
        *pValue = (NvU32)wincap->bHorizFilter;
        break;
    case NvDdkDispWindowAttribute_SupportsVerticalFiltering:
        *pValue = (NvU32)wincap->bVertFilter;
        break;
    case NvDdkDispWindowAttribute_SupportsPremultipliedAlpha:
        *pValue = (NvU32)wincap->bPremultipliedAlpha;
        break;
    case NvDdkDispWindowAttribute_Depth:
        *pValue = hWindow->attr.Depth;
        break;
    case NvDdkDispWindowAttribute_Rotation:
        *pValue = hWindow->attr.Rotation;
        break;
    case NvDdkDispWindowAttribute_Mirror:
        *pValue = hWindow->attr.Mirror;
        break;
    case NvDdkDispWindowAttribute_SourceRect_Left:
        *pValue = hWindow->attr.SourceRect.left;
        break;
    case NvDdkDispWindowAttribute_SourceRect_Top:
        *pValue = hWindow->attr.SourceRect.top;
        break;
    case NvDdkDispWindowAttribute_SourceRect_Right:
        *pValue = hWindow->attr.SourceRect.right;
        break;
    case NvDdkDispWindowAttribute_SourceRect_Bottom:
        *pValue = hWindow->attr.SourceRect.bottom;
        break;
    case NvDdkDispWindowAttribute_DestRect_Left:
        *pValue = hWindow->attr.DestRect.left;
        break;
    case NvDdkDispWindowAttribute_DestRect_Top:
        *pValue = hWindow->attr.DestRect.top;
        break;
    case NvDdkDispWindowAttribute_DestRect_Right:
        *pValue = hWindow->attr.DestRect.right;
        break;
    case NvDdkDispWindowAttribute_DestRect_Bottom:
        *pValue = hWindow->attr.DestRect.bottom;
        break;
    case NvDdkDispWindowAttribute_Location_X:
        *pValue = hWindow->attr.Location.x;
        break;
    case NvDdkDispWindowAttribute_Location_Y:
        *pValue = hWindow->attr.Location.y;
        break;
    case NvDdkDispWindowAttribute_Filtering:
        *pValue = hWindow->attr.HorizFiltering; // pick one
        break;
    case NvDdkDispWindowAttribute_Filtering_Horizontal:
        *pValue = hWindow->attr.HorizFiltering;
        break;
    case NvDdkDispWindowAttribute_Filtering_Vertical:
        *pValue = hWindow->attr.VertFiltering;
        break;
    case NvDdkDispWindowAttribute_DigitalVibrance:
        *pValue = hWindow->attr.DigitalVibrance;
        break;
    case NvDdkDispWindowAttribute_ScaleNicely:
        *pValue = hWindow->attr.ScaleNicely;
        break;
    case NvDdkDispWindowAttribute_BlendType:
        *pValue = hWindow->attr.BlendType;
        break;
    case NvDdkDispWindowAttribute_AlphaOperation:
        *pValue = hWindow->attr.AlphaOperation;
        break;
    case NvDdkDispWindowAttribute_AlphaDirection:
        *pValue = hWindow->attr.AlphaDirection;
        break;
    case NvDdkDispWindowAttribute_AlphaValue:
        *pValue = hWindow->attr.AlphaValue;
        break;
    case NvDdkDispWindowAttribute_ColorKey:
        *pValue = hWindow->attr.ColorKeyLower;
        break;
    case NvDdkDispWindowAttribute_ColorKeyLower:
        *pValue = hWindow->attr.ColorKeyLower;
        break;
    case NvDdkDispWindowAttribute_ColorKeyUpper:
        *pValue = hWindow->attr.ColorKeyUpper;
        break;
    case NvDdkDispWindowAttribute_ColorSpaceCoef_C11:
        *pValue = hWindow->attr.ColorSpaceCoef.C11;
        break;
    case NvDdkDispWindowAttribute_ColorSpaceCoef_C12:
        *pValue = hWindow->attr.ColorSpaceCoef.C12;
        break;
    case NvDdkDispWindowAttribute_ColorSpaceCoef_C13:
        *pValue = hWindow->attr.ColorSpaceCoef.C13;
        break;
    case NvDdkDispWindowAttribute_ColorSpaceCoef_C22:
        *pValue = hWindow->attr.ColorSpaceCoef.C22;
        break;
    case NvDdkDispWindowAttribute_ColorSpaceCoef_C23:
        *pValue = hWindow->attr.ColorSpaceCoef.C23;
        break;
    case NvDdkDispWindowAttribute_ColorSpaceCoef_C32:
        *pValue = hWindow->attr.ColorSpaceCoef.C32;
        break;
    case NvDdkDispWindowAttribute_ColorSpaceCoef_C33:
        *pValue = hWindow->attr.ColorSpaceCoef.C33;
        break;
    case NvDdkDispWindowAttribute_ColorSpaceCoef_K:
        *pValue = hWindow->attr.ColorSpaceCoef.K;
        break;
    default:
        err = NvError_BadParameter;
    }

    NvOsMutexUnlock( hWindow->hController->mutex );

    return err;
}

NvError
NvDdkDispControllerSuspend(
    NvDdkDispControllerHandle hController,
    NvU32 flags)
{
#if 0
    NvError e;
    NvDdkDispHal *hal;
    NvBool b;

    NV_ASSERT( hController );
    NV_ASSERT( flags == 0 );

    NvOsMutexLock( hController->mutex );

    hal = &hController->hal;

    if( hController->state == ControllerState_Suspend )
    {
        /* idempotent, we suppose; do nothing */
        e = NvError_DispSamePwrState;
        goto clean;
    }
    else if( hController->state == ControllerState_Active )
    {
        hal->HwBegin( hController->Index );

        /* suspend the panels */
        NvDdkDispPrivUpdatePanels( hController,
            NvOdmDispDevicePowerLevel_Suspend );

        b = hal->HwEnd( hController->Index, NVDDK_DISP_WAIT );
        if( !b )
        {
            e = NvError_BadParameter;
            goto clean;
        }

        /* suspend the controller */
        hal->HwSuspend( hController->Index );
    }

    hController->pre_suspend_state = hController->state;
    hController->state = ControllerState_Suspend;

    b = NvDdkDispPrivPowerHint( hController->hDisp );
    if( !b )
    {
        e = NvError_BadParameter;
        goto clean;
    }

    e = NvSuccess;

clean:
    NvOsMutexUnlock( hController->mutex );
    return e;
#endif
	return NvSuccess;
}

NvError
NvDdkDispControllerResume(
    NvDdkDispControllerHandle hController,
    NvU32 flags )
{
#if 0
    NvError e;
    NvDdkDispCapabilities *caps;
    NvDdkDispHal *hal;
    NvBool b;

    NV_ASSERT( hController );
    NV_ASSERT( flags == 0 ||
        !( (flags & ~NVDDK_DISP_WAIT
                  &  ~NVDDK_DISP_DEBUG )));

    caps = hController->hDisp->caps;

    NvOsMutexLock( hController->mutex );

    hal = &hController->hal;

    if( hController->state == ControllerState_Active )
    {
        /* idempotent, we suppose; do nothing */
        e = NvError_DispSamePwrState;
        goto clean;
    }
    else if( hController->state == ControllerState_Suspend &&
        hController->pre_suspend_state == ControllerState_Active &&
        /* the surface or mode could have been set to null while the
         * controller is suspended -- check for that.
         */
        NvDdkDispPrivIsModeNull( &hController->mode ) == NV_FALSE &&
        hController->nAttached &&
        hController->window_enable )
    {
        if( flags & NVDDK_DISP_DEBUG )
        {
            NvU32 i, j;

            NvOsDebugPrintf( "NvDdkDispControllerResume/ controller: %d\n",
                hController->Index );
            NvOsDebugPrintf( "mode: " );
            NvDdkDispPrivPrintMode( &hController->mode );
            for( i = 0; i < caps->NumWindows; i++ )
            {
                NvDdkDispWindow *win = &hController->windows[i];
                NvOsDebugPrintf( "window: %d num surfaces: %d\n",
                    i, win->nSurfaces );
                for( j = 0; j < win->nSurfaces; j++ )
                {
                    NvDdkDispPrivPrintSurface( &win->surface[j] );
                }
            }
        }

        /* resume the controller */
        b = hal->HwResume( hController->Index );
        if( !b )
        {
            goto fail;
        }

        /* restore all of the configuration */
        b = NvDdkDispPrivModeCommit( hController );
        if( !b )
        {
            goto fail;
        }
    }
    else
    {
        hController->state = ControllerState_Stopped;
    }

    b = NvDdkDispPrivPowerHint( hController->hDisp );
    if( !b )
    {
        goto fail;
    }

    hController->pre_suspend_state = hController->state;
    e = NvSuccess;
    goto clean;

fail:
    e = NvError_BadParameter;

clean:
    NvOsMutexUnlock( hController->mutex );
    return e;
#endif
	return NvSuccess;
}

NvError
NvDdkDispControllerSetCursor(
    NvDdkDispControllerHandle hController,
    NvPoint *position,
    NvSize *size,
    void *AndPlane,
    void *XorPlane,
    NvU32 fgColor,
    NvU32 bgColor,
    NvU32 flags )
{
    NvError err = NvSuccess;
    NvDdkDispHal *hal;
    NvU16 *xor;
    NvU16 *and;
    NvU32 offset;
    NvU32 i, j;
    NvRmMemHandle* newPlanes;

    NV_ASSERT( hController );
    NV_ASSERT( position );
    NV_ASSERT( size );
    NV_ASSERT( AndPlane );
    NV_ASSERT( XorPlane );

    // FIXME: check cursor cap bits
    NV_ASSERT( size->width == 32 || size->width == 64 );
    NV_DEBUG_CODE(
            if( size->width == 32 ) NV_ASSERT( size->height == 32 );
            if( size->width == 64 ) NV_ASSERT( size->height == 64 );
        );

    xor = (NvU16 *)XorPlane;
    and = (NvU16 *)AndPlane;

    NvOsMutexLock( hController->mutex );

    // Resolve where to write to
    if (hController->cursor.current == &hController->cursor.hMem1)
        newPlanes = &hController->cursor.hMem2;
    else
        newPlanes = &hController->cursor.hMem1;

    hal = &hController->hal;

    hController->cursor.size = *size;
    hController->cursor.position = *position;
    hController->cursor.fgColor = fgColor;
    hController->cursor.bgColor = bgColor;

    /* copy the And & Xor planes into the cursor surface */
    for( i = 0; i < (NvU32)size->height; i++ )
    {
        for( j = 0; j < (NvU32)size->width / 16; j++ )
        {
            /* 8 16-bit chunks per line, 2 chunks per 16 pixels */
            offset = ((8 * i) + (2 * j)) * 2;
            NvRmMemWr16( *newPlanes, offset, *and );

            offset += 2;
            NvRmMemWr16( *newPlanes, offset, *xor );

            and++;
            xor++;
        }
    }

    if( hController->state == ControllerState_Active )
    {
        NvBool b;
        hal->HwBegin( hController->Index );
        hal->HwSetCursor( hController->Index,
            *newPlanes,
            &hController->cursor.size,
            hController->cursor.fgColor,
            hController->cursor.bgColor );
        hal->HwSetCursorPosition( hController->Index,
            &hController->cursor.position );
        b = hal->HwEnd( hController->Index, flags & NVDDK_DISP_WAIT );
        // If operation fails, assume that new handle was not set
        if( !b )
        {
            err = NvError_BadParameter;
        }
        else if ( hController->cursor.pinned )
        {
            // New memory was set, unpin the old one
            NvRmMemUnpin( *hController->cursor.pinned );
            hController->cursor.pinned = newPlanes;
        }
    }
    // Store the new plane as current
    hController->cursor.current = newPlanes;

    NvOsMutexUnlock( hController->mutex );

    return err;
}

NvError
NvDdkDispControllerSetCursorPosition(
    NvDdkDispControllerHandle hController,
    NvPoint *pPosition,
    NvU32 flags)
{
    NvDdkDispHal *hal;
    NvBool full_update = NV_FALSE;

    NV_ASSERT( hController );

    NvOsMutexLock( hController->mutex );

    hal = &hController->hal;

    if( flags & NVDDK_DISP_CURSOR_HIDE )
    {
        pPosition = 0;
    }

    if( !pPosition )
    {
        if( hController->cursor.enable )
        {
            full_update = NV_TRUE;
        }

        hController->cursor.enable = NV_FALSE;
    }
    else
    {
        hController->cursor.position.x = pPosition->x;
        hController->cursor.position.y = pPosition->y;

        if( hController->cursor.enable == NV_FALSE )
        {
            full_update = NV_TRUE;
        }

        hController->cursor.enable = NV_TRUE;
    }

    if( hController->state == ControllerState_Active )
    {
        /* only need a begin/end if the cursor is being enabled or disabled.
         * steady-state position updates do not require begin/end.
         */
        if( full_update )
        {
            hal->HwBegin( hController->Index );
        }

        hal->HwSetCursorPosition( hController->Index, pPosition );

        if( full_update )
        {
            NvBool b;

            if( !pPosition && hController->cursor.pinned )
            {
                /* HAL is expected to pin the memory, unpin here */
                NvRmMemUnpin( *hController->cursor.pinned );
                hController->cursor.pinned = NULL;
            }

            (void)b;
            b = hal->HwEnd( hController->Index, flags & NVDDK_DISP_WAIT );
            NV_ASSERT( b );
        }
    }

    NvOsMutexUnlock( hController->mutex );

    return NvSuccess;
}

NvError
NvDdkDispSetContentProtection(
    NvDdkDispDisplayHandle hDisplay,
    NvDdkDispContentProtection prot,
    void *info,
    NvU32 size,
    NvU32 flags )
{
    NvError err = NvSuccess;
    NvDdkDispControllerHandle hController;
    NvDdkDispHal *hal;
    NvBool b;

    NV_ASSERT( hDisplay );
    NV_ASSERT( flags == 0 );

    NvOsMutexLock( hDisplay->mutex );

    hController = hDisplay->CurrentController;
    if (!hController)
    {
        NvOsMutexUnlock( hDisplay->mutex );
        return NvError_InvalidState;
    }

    NvOsMutexLock( hController->mutex );

    switch( hDisplay->attr.Type ) {
    case NvDdkDispDisplayType_HDMI:
        if( prot != NvDdkDispContentProtection_None &&
            prot != NvDdkDispContentProtection_HDCP )
        {
            err = NvError_BadParameter;
            goto clean;
        }
        break;
    case NvDdkDispDisplayType_TV:
        if( prot != NvDdkDispContentProtection_None &&
            prot != NvDdkDispContentProtection_Macrovision &&
            prot != NvDdkDispContentProtection_CGMSA )
        {
            err = NvError_BadParameter;
            goto clean;
        }
        break;
    default:
        err = NvError_DispTypeNotSupported;
        goto clean;
    }

    if( hController->state != ControllerState_Active )
    {
        if( prot == NvDdkDispContentProtection_None )
        {
            err = NvSuccess;
        }
        else
        {
            err = NvError_InvalidState;
        }
        goto clean;
    }

    /* no begin/end required */

    hal = &hController->hal;
    b = hal->HwContentProtection( hController->Index, prot, info,
        hal->TvHal, hal->HdmiHal );
    if( !b )
    {
        /* content protection has failed */
        err = NvError_DispAuthenticationFailed;
    }

clean:
    NvOsMutexUnlock( hController->mutex );
    NvOsMutexUnlock( hDisplay->mutex );
    return err;
}

NvError
NvDdkDispSetErrorTrigger(
    NvDdkDispControllerHandle hController,
    NvU32 bitfield,
    NvOsSemaphoreHandle hSem,
    NvU32 flags )
{
    NvDdkDispHal *hal;
    NvBool b;

    NV_ASSERT( hController );
    NV_ASSERT( flags == 0 );

    NV_DEBUG_CODE(
        if( bitfield && !hSem )
        {
            NV_ASSERT( !"semaphore must be null if bitfield is 0" );
        }
        if( hSem && !bitfield )
        {
            NV_ASSERT( !"bitfield must be non-zero if a semaphore is given" );
        }
    );

    NvOsMutexLock( hController->mutex );

    hal = &hController->hal;
    hal->HwBegin( hController->Index );

    hal->HwSetErrorTrigger( hController->Index, bitfield, hSem, flags );

    b = hal->HwEnd( hController->Index, 0 );

    NvOsMutexUnlock( hController->mutex );

    return ( b ) ? NvSuccess : NvError_BadValue;
}

NvU32
NvDdkDispGetLastActiveError(
    NvDdkDispControllerHandle hController )
{
    NvDdkDispHal *hal;
    NvU32 errs = 0;

    NV_ASSERT( hController );

    NvOsMutexLock( hController->mutex );

    if( hController->state != ControllerState_Active )
    {
        goto clean;
    }

    hal = &hController->hal;
    errs = hal->HwGetLastActiveError( hController->Index );

clean:
    NvOsMutexUnlock( hController->mutex );
    return errs;
}

static void NvDdkDispPrivPrintMode( NvOdmDispDeviceMode *om )
{
    NvOsDebugPrintf( "width: %d height: %d bpp: %d refresh: %d frequency: %d "
        "flags: 0x%x\n", om->width, om->height, om->bpp, om->refresh,
        om->frequency, om->flags );
}

static void NvDdkDispPrivPrintSurface( NvRmSurface *surf )
{
    NvOsDebugPrintf( "surface width: %d height: %d Bpp: %d "
        "layout: %s\n", surf->Width, surf->Height,
        NV_COLOR_GET_BPP(surf->ColorFormat),
        (surf->Layout == NvRmSurfaceLayout_Pitch) ? "pitch" : "tiled" );
}

/** test functions */
NvError
NvDdkDispTestCrcEnable(
    NvDdkDispControllerHandle hController,
    NvBool enable )
{
    NvDdkDispHal *hal;

    NV_ASSERT( hController );

    NvOsMutexLock( hController->mutex );

    hController->CrcEnabled = enable;

    if( hController->state == ControllerState_Active )
    {
        hal = &hController->hal;
        hal->HwBegin( hController->Index );
        hal->HwTestCrcEnable( hController->Index, enable );
        hal->HwEnd( hController->Index, 0 );
    }

    NvOsMutexUnlock( hController->mutex );

    return NvSuccess;
}

NvU32
NvDdkDispTestCrc( NvDdkDispControllerHandle hController )
{
    NvU32 ret = 0;
    NvDdkDispHal *hal;

    NV_ASSERT( hController );

    NvOsMutexLock( hController->mutex );

    if( hController->state == ControllerState_Active )
    {
        hal = &hController->hal;
        ret = hal->HwTestCrc( hController->Index );
    }

    NvOsMutexUnlock( hController->mutex );

    return ret;
}

NvU32
NvDdkDispTestHardwareId( NvDdkDispControllerHandle hController )
{
    NvDdkDispCapabilities *caps;

    NV_ASSERT( hController );

    caps = hController->hDisp->caps;
    return caps->HardwareId;
}

void
NvDdkDispTestInjectError( NvDdkDispControllerHandle hController, NvU32 error,
    NvU32 flags )
{
    NvDdkDispHal *hal;

    NV_ASSERT( hController );
    NV_ASSERT( flags == 0 );

    NvOsMutexLock( hController->mutex );

    if( hController->state == ControllerState_Active )
    {
        hal = &hController->hal;
        hal->HwTestInjectError( hController->Index, error, flags );
    }

    NvOsMutexUnlock( hController->mutex );
}
