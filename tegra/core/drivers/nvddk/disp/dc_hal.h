/*
 * Copyright (c) 2007-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef INCLUDED_DC_HAL_H
#define INCLUDED_DC_HAL_H

#include "nvos.h"
#include "nvassert.h"
#include "nvcommon.h"
#include "nvddk_disp.h"
#include "nvddk_disp_hw.h"
#include "nvodm_disp.h"
#include "nvrm_surface.h"
#include "nvrm_power.h"
#include "nvrm_channel.h"
#include "nvrm_hardware_access.h"
#include "arhost1x_uclass.h"
#include "t30/ardisplay.h"
#include "t30/ardisplay_b.h"
#include "class_ids.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define DC_HAL_CONTROLLERS 2
#define DC_HAL_WINDOWS 3
#define DC_HAL_MAX_COLOR_KEYS 2
#define DC_HAL_REG_BATCH_SIZE 32

/* this must be set to 1 AND NV_ENABLE_DEBUG_PRINTS in order to dump the
 * register accesses.
 */
#define DC_DUMP_REGS 0

#define DC_HAL_FLIP_WINDOW_WORDS    (64)
#define DC_HAL_FLIP_WINDOW_WAITS    (5)
#define DC_HAL_SURF_CACHE_SIZE      (4)

typedef struct DcSurfaceCacheRec
{
    NvRmMemHandle hSurfs[NVDDK_DISP_MAX_SURFACES];
    NvU32 nSurfs;
    NvU32 surf_addrs[NVDDK_DISP_MAX_SURFACES]; // from NvRmMemPin
} DcSurfaceCache;

typedef struct DcWindowRec
{
    NvRmMemHandle hSurfs[NVDDK_DISP_MAX_SURFACES];
    NvU32 nSurfs;
    NvU32 surf_addrs[NVDDK_DISP_MAX_SURFACES]; // from NvRmMemPin
    NvRect src_rect;
    NvRect dst_rect;
    NvPoint loc;

    DcSurfaceCache surf_cache[DC_HAL_SURF_CACHE_SIZE];
    NvU32 surf_cache_idx;

    NvU32 index;
    NvDdkDispBlendType blend_op;
    NvDdkDispAlphaOperation alpha_op;
    NvDdkDispAlphaBlendDirection alpha_dir;
    NvU32 depth;
    NvU8 alpha;

    NvBool bAlphaFormat;
    NvBool bDependentWeight;
    NvBool bHorizFiltering;
    NvBool bVertFiltering;

    NvU32 colorkey_lower;
    NvU32 colorkey_upper;
    /* hardware supports 2 color keys */
    NvU8 colorkey_number;

    NvBool invert_vertical;
    NvBool invert_horizontal;

    /* shadow */
    NvU32 win_options;

    NvBool bBlendWAR;
    NvU32 warWindow;
} DcWindow;

typedef struct DcControllerRec
{
    NvU32 index; // the controller number
    NvRmDeviceHandle hRm;
    NvDdkDispCapabilities *caps;
    NvBool bInitialized;
    NvBool bClocked;

    NvU32 SyncPointId;
    NvU32 SyncPointShadow;
    NvU32 VsyncSyncPointId;

    NvOsSemaphoreHandle sem;
    NvU32 PowerClientId;
    NvRmFreqKHz freq;

    /* hardware mutex for protection with avp. */
    NvU32 hw_mutex;
    /* frame trigger mutex */
    NvOsMutexHandle frame_mutex;

    /* stream and pushbuffer */
    NvRmChannelHandle ch;
    NvRmStream stream;
    NvData32 *pb;
    NvRmStream *stream_fw;  // stream used in FlipWindow

    /* from odm */
    NvU32 base;
    NvU32 align;
    NvU32 pinmap;
    NvU32 PwmOutputPin;

    NvBool bSafe;
    NvBool bFailed;
    NvBool bReopen;
    NvBool bNeedWaitStream;

    /* save the current mode */
    NvOdmDispDeviceMode mode;
    NvRmFreqKHz panel_freq;

    /* shadow */
    NvU32 interface_control;
    NvU32 signal_options0;
    NvU32 color_control;
    NvU32 mem_high_priority;
    NvU32 mem_high_priority_timer;
    NvU32 output_select3;

    /* win_a, win_b, win_c updated, enabled, etc */
    NvU32 win_dirty;
    NvU32 win_enable;

    /* blending updates -- blending is best handled in HwEnd after all
     * attributes have been set (depth, alpha, etc).
     */
    NvBool update_blend;

    DcWindow windows[DC_HAL_WINDOWS];

    /* mapped register aperture */
    void *registers;
    NvRmPhysAddr aperture_phys;
    NvU32 aperture_size;

    /* batch register writes */
    NvU32 regs[DC_HAL_REG_BATCH_SIZE];
    NvU32 vals[DC_HAL_REG_BATCH_SIZE];
    NvU32 *reg;
    NvU32 *val;
    NvU32 *fence;

    NvOsIntrMutexHandle IntrMutex;
    NvOsInterruptHandle InterruptHandle;
    NvU32 InterruptBits; /* from NvDdkDispActiveError */
    NvOsSemaphoreHandle InterruptSem;
    NvU32 InterruptActiveErrors;
    NvBool bErrorTriggerEnable;

    /* internal error handling */
    NvOsSemaphoreHandle InternalSem;
    NvU32 InternalError;
    NvBool bInternalErrorEnable;

    /* content protection flags */
    NvBool bHdcp;
    NvBool bMacrovision;
    NvBool bCgmsa;

    /* clock flags */
    NvBool bMipi;

    /* Smartdimmer flags */
    NvBool bSkipNext; /* Skip next brightness change because HW Bug 744178 */
    NvOsMutexHandle sdMutex;
} DcController;

typedef enum DcSetBacklightConfigPinRec
{
    DcSetBacklightConfigPin_Unspecified,
    DcSetBacklightConfigPin_LPW2_PM0,
    DcSetBacklightConfigPin_LPW0_PM0,
    DcSetBacklightConfigPin_LM0_PM0,
    DcSetBacklightConfigPin_LHP0_PM0,
    DcSetBacklightConfigPin_LHP2_PM0,
    DcSetBacklightConfigPin_LVP0_PM0,
    DcSetBacklightConfigPin_LDI_PM0,
    DcSetBacklightConfigPin_LHS_PM0,
    DcSetBacklightConfigPin_LSCK_PM0,
    DcSetBacklightConfigPin_LCS_PM0,
    DcSetBacklightConfigPin_LSPI_PM0,
    DcSetBacklightConfigPin_LM1_PM1,
    DcSetBacklightConfigPin_LPW1_PM1,
    DcSetBacklightConfigPin_LVP1_PM1,
    DcSetBacklightConfigPin_LHP1_PM1,
    DcSetBacklightConfigPin_LPP_PM1,
    DcSetBacklightConfigPin_LVS_PM1,
    DcSetBacklightConfigPin_LSDA_PM1,
    DcSetBacklightConfigPin_LDC_PM1,
} DcSetBacklightConfigPin;

typedef enum DcPwmOutputPinRec
{
    DcPwmOutputPin_Unspecified,
    DcPwmOutputPin_LM1_PM1,
    DcPwmOutputPin_LDC_PM1,
    DcPwmOutputPin_LPW0_PM0,
    DcPwmOutputPin_LPW1_PM1,
    DcPwmOutputPin_LPW2_PM0,
    DcPwmOutputPin_LM0_PM0,
} DcPwmOutputPin;

typedef struct DcPwmOutputPinInfoRec
{
    NvU32 port;
    NvU32 pin;
    DcPwmOutputPin InternalOutputPin;
} DcPwmOutputPinInfo;

extern DcController s_Controllers[DC_HAL_CONTROLLERS];

#define DC_HAL_WIN_A        (0UL)
#define DC_HAL_WIN_B        (1UL)
#define DC_HAL_WIN_C        (2UL)
#define DC_HAL_BACKGROUND   (3UL)
#define DC_HAL_CURSOR       (4UL)
#define DC_HAL_HDMI         (5UL)
#define DC_HAL_DSI          (6UL)
#define DC_HAL_TVO          (7UL)

#define DC_WIN_MASK( window ) \
    (1UL << (window))

#define DC_HAL_MAX_CONTROLLERS 2

#define DC_CONTROLLER( index, controller, bail ) \
    do { \
        if( (index) >= DC_HAL_MAX_CONTROLLERS ) \
        { \
            NV_ASSERT( !"bad controller index" ); \
            bail; \
        } \
        \
        (controller) = &s_Controllers[ (index) ]; \
        NV_ASSERT( (controller)->hRm ); \
    } while( 0 )

#define DC_SAFE( cont, bail ) \
    do { \
        if( !(cont)->bSafe || (cont)->bFailed ) \
        { \
            (cont)->bFailed = NV_TRUE; \
            bail; \
        } \
    } while( 0 )

#define DC_DIRTY( cont, window ) \
    do { \
        (cont)->win_dirty |= DC_WIN_MASK( (window) ); \
    } while( 0 )

#define DC_LOCK( cont ) \
    do { \
        DcWaitStream( (cont) ); \
        NvRmChannelModuleMutexLock( (cont)->hRm, (cont)->hw_mutex ); \
    } while( 0 )

#define DC_UNLOCK( cont ) \
    do { \
        NvRmChannelModuleMutexUnlock( (cont)->hRm, (cont)->hw_mutex ); \
    } while( 0 )

#define DC_BEGIN( cont ) \
    do { \
        if( NVDDK_DISP_CAP_IS_SET( cont->caps, \
                NvDdkDispCapBit_IndirectRegs ) ) \
        { \
            (cont)->stream.LastEngineUsed = NvRmModuleID_Display; \
            (cont)->stream.SyncPointID = (cont)->SyncPointId; \
            NVRM_STREAM_BEGIN( &(cont)->stream, (cont)->pb, \
                NVRM_STREAM_BEGIN_MAX_WORDS ); \
            \
            DcPushSetcl( (cont) ); \
            \
            /* lock display */ \
            NVRM_STREAM_PUSH_U( (cont)->pb, \
               NVRM_CH_OPCODE_ACQUIRE_MUTEX( (cont)->hw_mutex ) ); \
        } \
        else \
        { \
            DC_LOCK( (cont) ); \
        } \
    } while( 0 )

#define DC_END( cont ) \
    do { \
        if( NVDDK_DISP_CAP_IS_SET( (cont)->caps, \
                NvDdkDispCapBit_IndirectRegs ) ) \
        { \
            NVRM_STREAM_PUSH_U( (cont)->pb, \
                NVRM_CH_OPCODE_RELEASE_MUTEX( (cont)->hw_mutex ) ); \
            NVRM_STREAM_END( &(cont)->stream, (cont)->pb ); \
            (cont)->pb = 0; \
        } \
        else \
        { \
            DC_UNLOCK( (cont) ); \
        } \
    } while( 0 )

#define DC_FLUSH_DIRECT( cont ) \
    do { \
        if( (cont)->reg > &(cont)->regs[0] ) \
        { \
            DcRegWr( (cont), \
                (cont)->reg - &(cont)->regs[0], \
                (cont)->regs, \
                (cont)->vals  ); \
            (cont)->reg = &(cont)->regs[0]; \
            (cont)->val = &(cont)->vals[0]; \
        } \
    } while( 0 )

#define DC_FLUSH( cont ) \
    do { \
        if( NVDDK_DISP_CAP_IS_SET( (cont)->caps, \
                NvDdkDispCapBit_IndirectRegs ) ) \
        { \
            (cont)->stream.ClientManaged = NV_TRUE; \
            (cont)->stream.SyncPointsUsed = (cont)->SyncPointShadow; \
            NvRmStreamFlush( &(cont)->stream, 0 ); \
        } \
        else \
        { \
            DC_FLUSH_DIRECT( (cont) ); \
        } \
    } while( 0 )

#define DC_WRITE_DIRECT( cont, r, v ) \
    do { \
        if( (cont)->reg == (cont)->fence ) \
        { \
            DC_FLUSH_DIRECT( (cont) ); \
        } \
        if( DC_DUMP_REGS ) \
        { \
            NV_DEBUG_PRINTF(( "disp: %.8x %.8x\n", (r), (v) )); \
        } \
        *(cont)->reg = (r) * 4; \
        *(cont)->val = (v); \
        (cont)->reg++; \
        (cont)->val++; \
    } while( 0 )

#define DC_WRITE_STREAM( cont, r, v, pb ) \
    do { \
        NvU32 _x; \
        _x = NVRM_CH_OPCODE_NONINCR( (r), 1 ); \
        NVRM_STREAM_PUSH_U( (pb), _x ); \
        NVRM_STREAM_PUSH_U( (pb), (v) ); \
    } while( 0 )

#define DC_WRITE( cont, r, v ) \
    do { \
        if( NVDDK_DISP_CAP_IS_SET( cont->caps, \
            NvDdkDispCapBit_IndirectRegs ) ) \
        { \
            if( (cont)->pb ) \
            { \
                DC_WRITE_STREAM( (cont), (r), (v), (cont)->pb ); \
            } \
            else \
            { \
                NV_ASSERT( !"no controller pushbuffer" ); \
            } \
        } \
        else \
        { \
            DC_WRITE_DIRECT( (cont), (r), (v) ); \
        } \
    } while( 0 )

#define NVRM_AUTO_HOST_ENABLE   1
#if NVRM_AUTO_HOST_ENABLE

#define HOST_CLOCK_ENABLE( cont ) \
    NvRmPowerModuleClockControl( (cont)->hRm, \
        NvRmModuleID_GraphicsHost, (cont)->PowerClientId, NV_TRUE)

#define HOST_CLOCK_DISABLE( cont ) \
    NvRmPowerModuleClockControl( (cont)->hRm, \
        NvRmModuleID_GraphicsHost, (cont)->PowerClientId, NV_FALSE)

#else
#define HOST_CLOCK_ENABLE( cont )  do {} while(0)
#define HOST_CLOCK_DISABLE( cont ) do {} while(0)
#endif

#define INCR_MEM_REF( hMem ) \
    do { \
        NvU32 id; \
        NvRmMemHandle temp; \
        id = NvRmMemGetId( (hMem) ); \
        NvRmMemHandleFromId( id, &temp ); \
    } while( 0 )

/** private hal functions */

void DcRegWr( DcController *cont, NvU32 num, const NvU32 *offsets,
    const NvU32 *values );
void DcRegRd( DcController *cont, NvU32 num, const NvU32 *offsets,
    NvU32 *values );
void DcSetClockPower( NvRmDeviceHandle hRm, NvU32 power_id, NvU32 controller,
    NvBool enable, NvBool bReopen );
void DcEnable( DcController *cont, NvU32 window, NvBool enable );
void DcPushSetcl( DcController *cont );
void DcRegWrSafe( DcController *cont );
void DcRegWrSafe_Stream( DcController *cont );
void DcRegWrSafe_Direct( DcController *cont );
void DcLatch( DcController *cont, NvBool bForceDirect );
void DcSetLatchMux( DcController *cont );
void DcSetInterrupts( DcController *cont );
void DcSetMcConfig( DcController *cont );
void DcSetPanelPower( DcController *cont, NvBool enable );
void DcSuspend( DcController *cont );
void DcResume( DcController *cont );
void DcSetWindow( DcController *cont, NvU32 window );
void DcSetSurfaceAddress( DcController *cont, DcWindow *win, NvRect *src,
    NvRmSurface *surf, NvU32 count );
NvBool DcSetColorDepth( DcController *cont, DcWindow *win, NvRmSurface *surf,
    NvU32 count );
void DcSetColorBase( DcController *cont, NvU32 base );
void DcBlendWindows( DcController *cont, NvBool bReset );
void DcSetColorKey( DcController *cont, DcWindow *win );
void DcWaitFrame( DcController *cont );
void DcPushWaitFrame( DcController *cont );
void DcPushImmediate( DcController *cont );
void DcInterruptHandler( void *args );
void DcModeFlags( DcController *cont, NvDdkDispModeFlags flags );
void DcSetBacklight( DcController *cont, DcSetBacklightConfigPin conf,
    NvU8 intensity );
void DcStopDisplay( DcController *cont, NvBool bWaitFrame );
void DcWaitStream( DcController *cont );
void DcSurfaceDelete( DcWindow *win, NvU32 idx );
void DcSurfaceCacheClear( DcWindow *win );
void DcSurfaceCacheAdd( DcWindow *win );
NvBool DcSurfaceCacheFind( DcWindow *win, NvRmSurface *s, NvU32 count );
NvU32 DcGetInterruptType( DcController *cont );

/** exposed to ddk - implements function pointers in nvddk_disp_hal.h */

NvBool DcHal_HwOpen( NvRmDeviceHandle hRm, NvU32 controller,
    NvDdkDispCapabilities *caps, NvBool bReopen );
void DcHal_HwClose( NvU32 controller );
void DcHal_HwSuspend( NvU32 controller );
NvBool DcHal_HwResume( NvU32 controller );
void DcHal_HwBegin( NvU32 controller );
NvBool DcHal_HwEnd( NvU32 controller, NvU32 flags );
void DcHal_HwTriggerFrame( NvU32 controller );
void DcHal_HwCacheClear( NvU32 controller, NvU32 flags );
void DcHal_HwSetMode( NvU32 number, NvOdmDispDeviceMode *mode,
    NvDdkDispModeFlags flags );
void DcHal_HwSetBaseColorSize( NvU32 controller, NvU32 base );
void DcHal_HwSetDataAlignment( NvU32 controller, NvU32 align );
void DcHal_HwSetPinMap( NvU32 controller, NvU32 pinmap );
void DcHal_HwSetPwmOutputPin( NvU32 controller, NvU32 PwmOutputPin );
void DcHal_HwSetPinPolarities( NvU32 controller, NvU32 nPins,
    NvOdmDispPin *pins, NvOdmDispPinPolarity *vals );
void DcHal_HwSetPeriodicInstructions( NvU32 controller, NvU32 nInstructions,
    NvOdmDispCliInstruction *instructions );
void DcHal_HwSetWindowSurface( NvU32 controller, NvU32 window,
    NvRmSurface *surf, NvU32 count, NvRect *src, NvRect *dest, NvPoint *loc,
    NvU32 rot, NvDdkDispMirror mirror );
void DcHal_HwFlipWindowSurface( NvU32 controller, NvU32 window,
    NvRmSurface *surf, NvU32 count, NvRmStream *stream, NvRmFence *fence,
    NvU32 flags );
void DcHal_HwSetDepth( NvU32 controller, NvU32 window, NvU32 depth );
void DcHal_HwSetFiltering( NvU32 controller, NvU32 window, NvBool h_filter,
    NvBool v_filter );
void DcHal_HwSetVibrance( NvU32 controller, NvU32 window, NvU32 vib );
void DcHal_HwSetScaleNicely( NvU32 controller, NvU32 window, NvBool scale );
void DcHal_HwSetBlend( NvU32 controller, NvU32 window,
    NvDdkDispBlendType blend );
void DcHal_HwSetAlphaOp( NvU32 controller, NvU32 window,
    NvDdkDispAlphaOperation op, NvDdkDispAlphaBlendDirection dir );
void DcHal_HwSetAlpha( NvU32 controller, NvU32 window, NvU8 alpha );
void DcHal_HwSetCsc( NvU32 controller, NvU32 window,
    NvDdkDispColorSpaceCoef *csc );
void DcHal_HwSetColorKey( NvU32 controller, NvU32 window, NvU32 upper,
    NvU32 lower );
void DcHal_HwSetColorCorrection( NvU32 controller, NvU32 brightness,
    NvU32 gamma_red, NvU32 gamma_green, NvU32 gamma_blue,
    NvU32 scale_red, NvU32 scale_green, NvU32 scale_blue,
    NvU32 contrast );
void DcHal_HwSetBacklight( NvU32 controller, NvU8 intensity );
void DcHal_HwSetBackgroundColor( NvU32 controller, NvU32 val );
void DcHal_HwSetCursor( NvU32 controller, NvRmMemHandle mem, NvSize *size,
    NvU32 fgColor, NvU32 bgColor );
void DcHal_HwSetCursorPosition( NvU32 controller, NvPoint *pt );
NvBool DcHal_HwContentProtection( NvU32 controller, NvDdkDispContentProtection
    protection, void *info, NvDdkDispTvoHal *TvHal,
    NvDdkDispHdmiHal *HdmiHal );
void DcHal_HwAudioFrequency( NvU32 controller, NvDdkDispAudioFrequency freq );
void DcHal_HwDsiEnable( NvU32 controller, NvOdmDispDeviceHandle hPanel,
    NvU32 DsiInstance, NvBool enable);
void DcHal_HwDsiSetIndex( NvU32 controller, NvU32 DsiInstance,
    NvOdmDispDeviceHandle hPanel );
NvBool DcHal_HwDsiCommandModeUpdate( NvU32 controller, NvRmSurface *surf,
    NvU32 count, NvPoint *pSrc, NvRect *pUpdate,
    NvU32 DsiInstance, NvOdmDispDeviceHandle hPanel );
void DcHal_HwDsiSwitchMode( NvRmDeviceHandle hRm, NvU32 controller,
    NvU32 DsiInstance, NvOdmDispDeviceHandle hPanel);
void DcHal_SetErrorTrigger( NvU32 controller, NvU32 bitfield,
    NvOsSemaphoreHandle hSem, NvU32 flags );
NvU32 DcHal_HwGetLastActiveError( NvU32 controller );
void DcHal_HwSetErrorSem( NvU32 controller, NvOsSemaphoreHandle hSem );
NvU32 DcHal_HwGetLastError( NvU32 controller );
void DcHal_HwTestCrcEnable( NvU32 controller, NvBool enable );
NvU32 DcHal_HwTestCrc( NvU32 controller );
void DcHal_HwTestInjectError( NvU32 controller, NvU32 error, NvU32 flags );

#if defined(__cplusplus)
}
#endif

#endif
