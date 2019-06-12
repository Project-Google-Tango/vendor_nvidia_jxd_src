/*
 * Copyright (c) 2007-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef INCLUDED_NVDDK_DISP_STRUCTURE_H
#define INCLUDED_NVDDK_DISP_STRUCTURE_H

#include "nvcommon.h"
#include "nvddk_disp.h"
#include "nvodm_disp.h"
#include "nvddk_disp_hw.h"
#include "nvddk_disp_edid.h"
#include "nvos.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * Note on synchronization:
 *      Both Controllers and Displays have mutexes.  Displays must be locked
 *      BEFORE Controllers to ensure proper lock ordering.
 */

#define NVDDK_DISP_MAX_ATTACHED_DISPLAYS 3

typedef struct WindowAttrRec
{
    NvU32 Number;
    NvDdkDispWindowUsage Usage;
    NvU32 Depth;
    NvU32 Rotation;
    NvDdkDispMirror Mirror;
    NvRect SourceRect;
    NvRect DestRect;
    NvPoint Location;
    NvBool HorizFiltering;
    NvBool VertFiltering;
    NvU32 DigitalVibrance;
    NvBool ScaleNicely;
    NvDdkDispBlendType BlendType;
    NvDdkDispAlphaOperation AlphaOperation;
    NvDdkDispAlphaBlendDirection AlphaDirection;
    NvU8 AlphaValue;
    NvU32 ColorKeyLower;
    NvU32 ColorKeyUpper;
    NvDdkDispColorSpaceCoef ColorSpaceCoef;
} WindowAttr;

typedef struct NvDdkDispWindowRec
{
    /* attributes */
    WindowAttr attr;

    /* scan-out surface(s) */
    NvRmSurface surface[NVDDK_DISP_MAX_SURFACES];
    NvU32 nSurfaces;

    /* for window_enable in controller */
    NvU32 mask;

    NvDdkDispControllerHandle hController;
    NvDdkDispHandle hDisp;

    NvBool bColorKeyDirty;
    NvBool bDirty; // used with the NVDDK_DISP_DO_NOT_COMMIT flag
} NvDdkDispWindow;

typedef struct DisplayAttrRec
{
    NvDdkDispDisplayType Type;
    NvDdkDispDisplayUsageType Usage;
    NvU32 MaxHorizontalResolution;
    NvU32 MaxVerticalResolution;
    NvU32 ColorPrecision;
    NvDdkDispBacklightControl Backlight;
    NvU32 Brightness;
    NvU32 GammaRed;
    NvU32 GammaGreen;
    NvU32 GammaBlue;
    NvU32 ScaleRed;
    NvU32 ScaleGreen;
    NvU32 ScaleBlue;
    NvU32 Contrast;
    NvU32 BacklightTimeout;
    NvDdkDispAudioFrequency AudioFrequency;
    NvU32 PartialModeLineOffset;
    NvU8 OutBacklightIntensity;
    NvU8 InBacklightIntensity;          // The "target" backlight intensity set by user.
    NvBool AutoBacklight;
    NvOdmDispDsiMode mode;
    NvU32 TV_HorizontalPosition;
    NvU32 TV_VerticalPosition;
    NvU32 TV_Overscan;
    NvU32 TV_OverscanY;
    NvU32 TV_OverscanCb;
    NvU32 TV_OverscanCr;
    NvDdkDispTvOutput TV_OutputFormat;
    NvDdkDispTvType TV_Type;
    NvU8 TV_DAC_Amplitude;
    NvDdkDispTvFilterMode TV_FilterMode;
    NvDdkDispTvScreenFormat TV_ScreenFormat;

    NvU32 DsiInstance;
} DisplayAttr;

struct NvDdkDispPanelRec;

typedef struct NvDdkDispDisplayRec
{
    /* attributes */
    DisplayAttr attr;

    /* EDID for plug-n-play */
    NvDdkDispEdid edid;

    /* synchronization */
    NvOsMutexHandle mutex;

    /* current controller (attached) */
    NvDdkDispControllerHandle CurrentController;

    /* physical display device */
    struct NvDdkDispPanelRec *panel;

    /* the current power level */
    NvOdmDispDevicePowerLevel power;

    /* the ddk handle */
    NvDdkDispHandle hDisp;

    /* which controllers this display can attach to (bitmask) */
    NvU8 AttachableControllers;
} NvDdkDispDisplay;

struct NvDdkDispTimeoutThreadRec;
struct NvDdkDispErrorThreadRec;
struct NvDdkDispSmartDimmerThreadRec;

typedef enum
{
    ControllerState_Stopped,
    ControllerState_Active,
    ControllerState_Suspend,

    ControllerState_Num,
    ControllerState_Force32 = 0x7FFFFFFF,
} ControllerState;

typedef struct CursorRec
{
    NvSize size;
    NvRmMemHandle hMem1;
    NvRmMemHandle hMem2;
    NvPoint position;
    NvBool enable;
    NvRmMemHandle* current;
    NvRmMemHandle* pinned;
    NvU32 fgColor;
    NvU32 bgColor;
} Cursor;

typedef struct ControllerAttrRec
{
    NvU32 BackgroundColor;
    NvU8 SmartDimmer_Aggressiveness;
    NvU8 SmartDimmer_BinWidth;
    NvU8 SmartDimmer_HwUpdateDelay;
    NvU16 SmartDimmer_Csc_Coeff_R;
    NvU16 SmartDimmer_Csc_Coeff_G;
    NvU16 SmartDimmer_Csc_Coeff_B;
    NvBool SmartDimmer_Man_K_Enable;
    NvU16 SmartDimmer_Man_K_R;
    NvU16 SmartDimmer_Man_K_G;
    NvU16 SmartDimmer_Man_K_B;
    NvBool SmartDimmer_VideoLuminance;
    NvU8 SmartDimmer_Enable;
    NvU8 SmartDimmer_FlickerTimeLimit;
    NvU8 SmartDimmer_FlickerThreshold;
    NvU16 SmartDimmer_BlStep;
    NvU16 SmartDimmer_BlTimeConstant;
} ControllerAttr;

typedef struct NvDdkDispControllerRec
{
    /* attributes */
    ControllerAttr attr;

    NvDdkDispWindow windows[NVDDK_DISP_MAX_WINDOWS];
    Cursor cursor;
    NvU32 window_enable;

    /* synchronization */
    NvOsMutexHandle mutex;

    /* current mode (and timing) */
    NvOdmDispDeviceMode mode;
    NvDdkDispModeFlags mode_flags;
    NvBool bModeDirty;

    /* marketing-limited maximum frequency */
    NvS32 MaxFrequency;

    /* various high-level states */
    ControllerState state;
    ControllerState pre_suspend_state;

    /* current displays */
    NvDdkDispDisplayHandle AttachedDisplays[NVDDK_DISP_MAX_ATTACHED_DISPLAYS];
    NvU32 nAttached;

    /* bit mask value for the attachable controller state in the display */
    NvU8 AttachMask;

    /* the controller number (hardware) */
    NvU32 Index;

    /* initialize bit for hardware */
    NvBool HwInitialized;

    /* HAL structure */
    NvDdkDispHal hal;

    /* handles backlight timeout */
    struct NvDdkDispTimeoutThreadRec *timeout_thread;

    /* handles errors from hardware (tries to, anyway) */
    struct NvDdkDispErrorThreadRec *error_thread;

    /* smart dimmer to save power, hopefully */
    struct NvDdkDispSmartDimmerThreadRec *smartdimmer_thread;

    /* crc enable bit */
    NvBool CrcEnabled;

    /* the ddk handle */
    NvDdkDispHandle hDisp;
} NvDdkDispController;

typedef struct NvDdkDispTimeoutThreadRec
{
    NvOsSemaphoreHandle sem;
    NvU32 timeout;
    NvDdkDispControllerHandle hController;
    NvOsThreadHandle thread;
    NvDdkDispHal *hal;
    NvBool bShutdown;
} NvDdkDispTimeoutThread;

typedef struct NvDdkDispErrorThreadRec
{
    NvOsSemaphoreHandle sem;
    NvDdkDispControllerHandle hController;
    NvOsThreadHandle thread;
    NvDdkDispHal *hal;
    NvBool bShutdown;
} NvDdkDispErrorThread;

typedef struct NvDdkDispSmartDimmerThreadRec
{
    NvOsSemaphoreHandle sem;
    NvDdkDispControllerHandle hController;
    NvOsThreadHandle thread;
    NvDdkDispHal *hal;
    NvBool bShutdown;
} NvDdkDispSmartDimmerThread;

typedef struct NvDdkDispPanelRec
{
    NvOdmDispDeviceHandle hPanel;
    NvOdmDispDeviceMode modes[NVDDK_DISP_MAX_MODES];
    NvU32 nModes;

    /* could be a built-in device */
    NvDdkDispBuiltin *builtin;

    /* attributes from ODM */
    NvOdmDispDeviceUsageType Usage;
    NvOdmDispDeviceType Type;
    NvU32 MaxHorizontalResolution;
    NvU32 MaxVerticalResolution;
    NvU32 BaseColorSize;
    NvU32 DataAlignment;
    NvU32 PinMap;
    NvU32 ColorCalibrationRed;
    NvU32 ColorCalibrationGreen;
    NvU32 ColorCalibrationBlue;
    NvU32 PwmOutputPin;
    NvU8 BacklightInitialIntensity;

    /* extended configuration */
    void *Config;

    /* pointer back into the original panel array */
    NvU32 DeviceIndex;
    NvU32 DsiInstance;
} NvDdkDispPanel;

typedef struct NvDdkDispRec
{
    NvRmDeviceHandle hDevice;
    NvOsMutexHandle mutex;
    NvU32 refcount;
    NvDdkDispCapabilities *caps;
    NvU32 PowerClientId;

    NvDdkDispController controllers[NVDDK_DISP_MAX_CONTROLLERS];
    NvDdkDispDisplay displays[NVDDK_DISP_MAX_DISPLAYS];

    NvDdkDispPanel panels[NVDDK_DISP_MAX_DISPLAYS];
    /* copy of all the panel handles */
    NvOdmDispDeviceHandle hPanels[NVDDK_DISP_MAX_DISPLAYS];
    NvU32 nPanels;
    NvU32 nOdmPanels;

    NvBool bReopen;
    NvBool bDsiPresent;
} NvDdkDisp;

#if defined(__cplusplus)
}
#endif

#endif
