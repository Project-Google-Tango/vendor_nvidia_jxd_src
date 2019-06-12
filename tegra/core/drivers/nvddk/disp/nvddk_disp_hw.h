/*
 * Copyright (c) 2007 - 2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVDDK_DISP_HW_H
#define INCLUDED_NVDDK_DISP_HW_H

/**
 * Hardware abstraction layer for the display ddk.
 *
 * Updates to hardware are done in an atomic manner.  HwBegin() must
 * be called before any other attribute update functions.  HwEnd() will commit
 * the changes.
 */

#include "nvcommon.h"
#include "nvcolor.h"
#include "nvrm_init.h"
#include "nvddk_disp.h"
#include "nvddk_disp_edid.h"
#include "nvodm_disp.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/** Display DDK capabilities */

typedef enum
{
    /* portal player's display hardware */
    NvDdkDispEngine_Dvo = 1,
    /* nvidia display hardware from ar/ap tree. */
    NvDdkDispEngine_Dc = 2,

    NvDdkDispEngine_Num,
    NvDdkDispEngine_Force32 = 0x7FFFFFFF,
} NvDdkDispEngine;

typedef enum
{
    NvDdkDispModeFlag_None = 0,
    NvDdkDispModeFlag_TV = 1,
    NvDdkDispModeFlag_CRT = 2,
    NvDdkDispModeFlag_MIPI = 4,

    NvDdkDispModeFlag_DisableClockInBlank = 8,
    NvDdkDispModeFlag_DataEnableMode = 16,

    NvDdkDispModeFlag_Force32 = 0x7FFFFFFF,
} NvDdkDispModeFlags;

typedef enum
{
    NvDdkDispCapBit_Cursor_x32 = 0,
    NvDdkDispCapBit_Cursor_x64,
    NvDdkDispCapBit_Hdmi,
    NvDdkDispCapBit_Tv,
    NvDdkDispCapBit_Hdtv_Analog,
    NvDdkDispCapBit_Crt,
    NvDdkDispCapBit_Hdmi_Kfuse,
    NvDdkDispCapBit_Hdmi_PreEmphasis,

    /* hardware windows are accessed indirectly, must use streams to
     * synchronize.
     */
    NvDdkDispCapBit_IndirectRegs,

    /* supports Smart Dimmer */
    NvDdkDispCapBit_SmartDimmer,

    /* line buffer of limited size */
    NvDdkDispCapBit_LineBuffer_1280,

    /* stride restrictions for yuv surfaces */
    NvDdkDispCapBit_Bug_363177,

    /* ap1x chips don't deal with bandwidth very well */
    NvDdkDispCapBit_Bug_BandwidthPowerHint,

    /* ap1x chips don't deal with underflow very well either */
    NvDdkDispCapBit_Bug_UnderflowReset,

    /* smart dimmer automatic mode may be broken */
    NvDdkDispCapBit_Bug_SmartDimmer_AutomaticMode,

    /* smart dimmer histogram may not be reset on SD_Disable */
    NvDdkDispCapBit_Bug_SmartDimmer_Histogram_Reset,

    /* double sync point increment bug - 580685 */
    NvDdkDispCapBit_Bug_Double_Syncpt_Incr,

    /* display PWM period counter too small bug - 722394 */
    NvDdkDispCapBit_Bug_722394,

    /* underflow line flush bug - 588475 */
    NvDdkDispCapBit_Bug_Disable_Underflow_Line_Flush,

    /* Force DSI Pclk divider to be 1 for T30 emulation */
    NvDdkDispCapBit_Dsi_ForcePclkDiv1,

    /* Force external display to default 480p mode which
     * is only mode supported on emulation
     */
    NvDdkDispCapBit_ForceDefaultExternalDisplayMode,

    /* Ratio CveFreq:TvoFreq=1:2 */
    NvDdkDispCapBit_RatioCveTvoFreq_1_2,

    NvDdkDispCapBit_Num,
    NvDdkDispCapBit_Force32 = 0x7FFFFFFF
} NvDdkDispCapBit;

typedef struct NvDdkDispWindowCapRec
{
    NvColorFormat *formats;
    NvYuvColorFormat *yuv_formats;
    NvBool bCsc;
    NvBool bHorizFilter;
    NvBool bVertFilter;
    NvBool bPremultipliedAlpha;
} NvDdkDispWindowCap;

typedef struct NvDdkDispCapabilitiesRec
{
    NvU32 NumControllers;
    NvU32 NumDisplays;
    NvU32 NumWindows;
    NvDdkDispEngine engine;
    NvDdkDispWindowCap *WindowCaps;
    NvU32 HardwareId; // only used to match CRCs
    NvU32 Bits[(NvDdkDispCapBit_Num + 31) / 32];
} NvDdkDispCapabilities;

#define NVDDK_DISP_CAP_IS_SET( caps, x ) \
    (((caps)->Bits[(x)/32] & (1U << ((x) & 31U ))) ? NV_TRUE : NV_FALSE)

#define NVDDK_DISP_CAP_SET( caps, x ) \
    ((caps)->Bits[(x)/32] |= (1U << ((x) & 31U )))

#define NVDDK_DISP_CAP_CLEAR( caps, x ) \
    ((caps)->Bits[(x)/32] &= ~(1U << ((x) & 31U )))

/* handy color space organization */
typedef struct NvDdkDispColorSpaceCoefRec
{
    NvS32 K;
    NvS32 C11, C12, C13;
    NvS32      C22, C23;
    NvS32      C32, C33;
} NvDdkDispColorSpaceCoef;

/* Smartdimmer settings structure. */
typedef struct NvDdkSmartDimmerSettingsRec
{
    NvU8 Enable;
    NvU8 BinWidth;
    NvU8 Aggressiveness;
    NvU8 HwUpdateDelay;
    NvBool bEnableVidLum;
    NvU16 Coeff_R, Coeff_G, Coeff_B;
    NvBool bEnableManK;
    NvU16 ManK_R, ManK_G, ManK_B;
    NvU8 FlickerTimeLimit, FlickerThreshold;
    NvU16 BlStep, BlTimeConstant;
} NvDdkSmartDimmerSettings;

/* built-in display table (not ODM customizable) */
typedef struct NvDdkDispBuiltinRec
{
    void (*Enable)( NvU32 controller, NvBool enable );
    void (*ListModes)( NvU32 *nModes, NvOdmDispDeviceMode *modes );
    void (*SetPowerLevel)( NvOdmDispDevicePowerLevel level );
    void (*GetParameter)( NvOdmDispParameter param, NvU32 *val );
    NvU64 (*GetGuid)( void );
    void (*SetEdid)( NvDdkDispEdid *edid, NvU32 flags );
    // ignore edid modes, use the built-in modes instead
#define NVDDK_DISP_SET_EDID_FLAG_USE_BUILTIN    (0x1)
#define NVDDK_DISP_SET_EDID_FLAG_USE_VESA       (0x2)
#define NVDDK_DISP_SET_EDID_FLAG_USE_ALL        (0x4)
#define NVDDK_DISP_SET_EDID_FLAG_USE_DEFAULT    (0x8)

    void (*AudioFrequency)( NvU32 controller, NvDdkDispAudioFrequency freq );
} NvDdkDispBuiltin;

typedef struct NvDdkDispTvoHalRec
{
    /* no Begin/End required for HwTvType -- no controller either -- this
     * will affect all TVs in the system, if there are ever more than one.
     */
    void (*HwTvType)( NvDdkDispTvType Type );

    void (*HwTvAttrs)( NvU32 controller, NvU32 Overscan, NvU32 OverscanY,
        NvU32 OverscanCb, NvU32 OverscanCr, NvDdkDispTvOutput OutputFormat,
        NvU8 DacAmplitude, NvDdkDispTvFilterMode FilterMode,
        NvDdkDispTvScreenFormat ScreenFormat);

    void (*HwTvPosition)( NvU32 controller, NvU32 hpos, NvU32 vpos );

    /* macrovision hal */
    void (*HwTvMacrovisionSetContext)( NvU32 controller,
        NvDdkDispMacrovisionContext *context );
    NvBool (*HwTvMacrovisionEnable)( NvU32 controller, NvBool enable );
    void (*HwTvMacrovisionSetCGMSA_20)( NvU32 controller, NvU8 cgmsa );
    void (*HwTvMacrovisionSetCGMSA_21)( NvU32 controller, NvU8 cgmsa );
} NvDdkDispTvoHal;

typedef struct NvDdkDispHdmiHalRec
{
    NvBool (*HwHdcpOp)( NvU32 controller, NvDdkDispHdcpContext *context,
        NvBool enable );
} NvDdkDispHdmiHal;

/* hardware abstraction function pointer table */
typedef struct NvDdkDispHalRec
{
    NvBool (*HwOpen)( NvRmDeviceHandle hRm, NvU32 controller,
        NvDdkDispCapabilities *caps, NvBool bReopen );
    void (*HwClose)( NvU32 controller );

    /**
     * HwSuspend/HwResume manage the hw power state only. Modes, surfaces,
     * attributes, etc., will be restored by the higher-level controller
     * by calling the appropriate Hw* function after HwResume is called.
     */
    void (*HwSuspend)( NvU32 controller );
    NvBool (*HwResume)( NvU32 controller );

    /**
     * Start a transaction to hardware. Only one transaction per controller
     * may be active at a time.
     *
     * @param controller The controller number
     */
    void (*HwBegin)( NvU32 controller );

    /**
     * Commits changes to hardware.
     *
     * If any of the changes from the last HwBegin() failed, then this
     * will return an error -- none of the changes will be applied to hardware.
     *
     * @param controller The controller number
     * @param flags Flag bits, may be zero or NVDDK_DISP_WAIT
     */
    NvBool (*HwEnd)( NvU32 controller, NvU32 flags );

    /**
     * Generates a frame for non-continuous modes with tearing-effect.
     */
    void (*HwTriggerFrame)( NvU32 controller );

    /* no begin/end required */
    void (*HwCacheClear)( NvU32 controller, NvU32 flags );

    void (*HwSetMode)( NvU32 controller, NvOdmDispDeviceMode *mode,
        NvDdkDispModeFlags flags );

    void (*HwSetBaseColorSize)( NvU32 controller, NvU32 base );

    void (*HwSetDataAlignment)( NvU32 controller, NvU32 align );

    void (*HwSetPinMap)( NvU32 controller, NvU32 pinmap );
    void (*HwSetPwmOutputPin)( NvU32 controller, NvU32 PwmOutputPin );

    void (*HwSetPinPolarities)( NvU32 controller, NvU32 nPins,
        NvOdmDispPin *pins, NvOdmDispPinPolarity *vals );

    void (*HwSetPeriodicInstructions)( NvU32 controller, NvU32 nInstructions,
        NvOdmDispCliInstruction *instructions );

    void (*HwSetWindowSurface)( NvU32 controller, NvU32 window,
        NvRmSurface *surf, NvU32 count, NvRect *src, NvRect *dst,
        NvPoint *loc, NvU32 rotation, NvDdkDispMirror mirror );

    /**
     * Fast surface flip. Does not require HwBegin/HwEnd.
     */
    void (*HwFlipWindowSurface)( NvU32 controller, NvU32 window,
        NvRmSurface *surf, NvU32 count, NvRmStream *stream, NvRmFence *fence,
        NvU32 flags );

    void (*HwSetDepth)( NvU32 controller, NvU32 window, NvU32 depth );

    void (*HwSetFiltering)( NvU32 controller, NvU32 window, NvBool h_filter,
        NvBool v_filter );

    void (*HwSetVibrance)( NvU32 controller, NvU32 window, NvU32 vib );

    void (*HwSetScaleNicely)( NvU32 controller, NvU32 window, NvBool scale );

    void (*HwSetBlend)( NvU32 controller, NvU32 window,
        NvDdkDispBlendType blend );

    void (*HwSetAlphaOp)( NvU32 controller, NvU32 window,
        NvDdkDispAlphaOperation op, NvDdkDispAlphaBlendDirection dir );

    void (*HwSetAlpha)( NvU32 controller, NvU32 window, NvU8 alpha );

    void (*HwSetCsc)( NvU32 controller, NvU32 window,
        NvDdkDispColorSpaceCoef *csc );

    void (*HwSetColorKey)( NvU32 controller, NvU32 window, NvU32 upper,
        NvU32 lower );

    void (*HwSetColorCorrection)( NvU32 controller, NvU32 brightness,
        NvU32 gamma_red, NvU32 gamma_green, NvU32 gamma_blue,
        NvU32 scale_red, NvU32 scale_green, NvU32 scale_blue, NvU32 contrast );

    void (*HwSetBacklight)( NvU32 controller, NvU8 intensity );

    void (*HwSetBackgroundColor)( NvU32 controller, NvU32 val );

    void (*HwSetCursor)( NvU32 controller, NvRmMemHandle mem, NvSize *size,
        NvU32 fgColor, NvU32 bgColor );
    void (*HwSetCursorPosition)( NvU32 controller, NvPoint *pt );

    /* Enables/Disables the DSI controller */
    void (*HwDsiEnable)( NvU32 controller, NvOdmDispDeviceHandle hPanel,
        NvU32 DsiInstance, NvBool enable);

    /* DSI transport calls may come in before dsi video is enabled */
    void (*HwDsiSetIndex)( NvU32 controller, NvU32 DsiInstance, NvOdmDispDeviceHandle hPanel );

    /* Updates the DSI display in command mode */
    NvBool (*HwDsiCommandModeUpdate)( NvU32 controller,
        NvRmSurface *surf, NvU32 count, NvPoint *pSrc, NvRect *pUpdate,
        NvU32 DsiInstance, NvOdmDispDeviceHandle hPanel );

    void (*HwDsiSwitchMode)( NvRmDeviceHandle hRm, NvU32 controller,
        NvU32 DsiInstance, NvOdmDispDeviceHandle hPanel);

    /* smart dimmer */
    void (*HwSmartDimmerConfig)( NvU32 controller,
            NvDdkSmartDimmerSettings *sdSettings, NvBool *manual );
    void (*HwSmartDimmerBrightness)( NvU32 controller, NvU8 *brightness );
    void (*HwSmartDimmerLog)( NvU32 controller, NvU32 InIntensity,
        NvU32 OutIntensity, NvBool show );

    /* no begin/end required */
    NvBool (*HwContentProtection)( NvU32 controller, NvDdkDispContentProtection
        protection, void *info, NvDdkDispTvoHal *TvHal,
        NvDdkDispHdmiHal *HdmiHal );

    void (*HwSetErrorTrigger)( NvU32 controller, NvU32 bitfield,
        NvOsSemaphoreHandle hSem, NvU32 flags );
    /* no begin/end required */
    NvU32 (*HwGetLastActiveError)( NvU32 controller );

    /* for internal error handling only */
    void (*HwSetErrorSem)( NvU32 controller, NvOsSemaphoreHandle hSem );
    NvU32 (*HwGetLastError)( NvU32 controller );

    /* test functions */
    void (*HwTestCrcEnable)( NvU32 controller, NvBool enable );
    /* retrieves the crc over the active data pins, doesn't need begin/end */
    NvU32 (*HwTestCrc)( NvU32 controller );
    void (*HwTestInjectError)( NvU32 controller, NvU32 error, NvU32 flags );

    /* Built-In devices */
    NvDdkDispBuiltin *Tv;
    NvDdkDispBuiltin *Crt;
    NvDdkDispBuiltin *Hdmi;

    /* special hals */
    NvDdkDispTvoHal *TvHal;
    NvDdkDispHdmiHal *HdmiHal;
    NvOdmDispDsiTransport *DsiTransport;
} NvDdkDispHal;

/**
 * Initialize a hal function pointer table.
 *
 * @param caps The controller capability bits
 * @param hal Function table for the HAL.
 */
void
NvDdkDispHalInit( NvDdkDispCapabilities *caps, NvDdkDispHal *hal );

#if defined(__cplusplus)
}
#endif

#endif
