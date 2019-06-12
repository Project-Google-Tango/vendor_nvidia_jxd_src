/*
 * Copyright (c) 2007-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * @brief <b>nVIDIA OpenMax video renderer component:
 *           video renderer</b>
 *
 * @b Description: Implements the NVAP 2D nvddk version of the OpenMax video renderer
 */


/**
 *   Note: The video renderer will render to whatever surface is provided
 *   as the destination surface (set by pNativeRender in the PortDef).
 *   If the native renderer device is not set, the video renderer will
 *   default to the primary surface
 */



/* =========================================================================
 *                     I N C L U D E S
 * ========================================================================= */

#include "common/NvxComponent.h"
#include "nvxvideorenderer.h"
#include <math.h>
#include "common/NvxTrace.h"
#include "common/NvxHelpers.h"
#include "common/NvxWindowManager.h"
#include "common/nvxcrccheck.h"

#include "nvassert.h"
#include "nvos.h"
#include "nvcommon.h"
#include "nvrm_surface.h"
#include "nvddk_2d_v2.h"
#include "nvmm.h"
#include "nvrm_power.h"
#include "nvapputil.h"
#include "nvxjitter.h"

#ifdef BUILD_GOOGLETV
#include <media/openmax_1.2/OMX_CoreExt.h>
#endif

typedef struct NVX_VIDREND_METRIC
{
    NvU32 attemptedFrames;
    NvU32 renderedFrames;
    NvU32 delayedFrames;
    NvU32 starvationRequest;
    NvU32 droppedFrames;
    NvU32 lateFrames;
} NVX_VIDREND_METRIC;

/* =========================================================================
 *                     D E F I N E S
 * ========================================================================= */

/* Renderer State information */
typedef struct SNvxVideoRenderData
{
    NvRmDeviceHandle hRmDevice;
    NvDdk2dHandle    h2dHandle;
    NvDdk2dBlitParameters TexParam;
    NvU32 dfsClient;

    // Render information
    NvxSurface *     pDestSurface;
    NvxSurface *     pSrcSurface;
    NvRmSurface      oBrushSurface;
    NvRect           rtDestRect;

    NvRmFence        oFences[NVDDK_2D_MAX_FENCES];
    NvxPort *        pInPort;

    // Note: OMX input color format could potentially be different from
    // the actual surface color format that we're using internally.
    // This would allow certain optimizations (like using YUV surfaces for video
    // display, even though the OMX renderer accepts RGB data for blt)
    NvColorFormat    eRequestedColorFormat;
    //NvColorFormat    eSurfaceColorFormat;

    // State information
    OMX_CONFIG_POINTTYPE    oPosition;
    OMX_MIRRORTYPE          eMirror;
    NvS32                   nRotation;
    NvS32                   nSetRotation;
    OMX_CONFIG_COLORKEYTYPE oColorKey;
    OMX_COLORBLENDTYPE      eColorBlend;
    OMX_U32                 nUniformAlpha;
    OMX_BOOL                bColorKeySet;
    OMX_BOOL                bNeedUpdate;
    OMX_BOOL                bPaused;
    OMX_BOOL                bUsingWindow;
    ENvxVidRendType         oRenderType;
    NvBool                  bInternalSurfAlloced;
    NvBool                  bSizeSet;
    OMX_FRAMESIZETYPE       oOutputSize;
    NvBool                  bOutputSizeSet;

    NvRect                  rtUserCropRect;
    NvBool                  bUserCropRectSet;

    NvRect                  rtActualCropRect;
    NvBool                  bCropSet;

    OMX_BOOL                bSentFirstFrameEvent;

    NVX_RENDERTARGETTYPE    eRenderMode;
    NvDdk2dSurface *        pDstDdk2dSurface;

    OMX_BUFFERHEADERTYPE   *pLastFrame;
    NvBool                  bCrcDump;
    NvOsFileHandle          fpCRC;
    NvU32                   nFrameCount;

    NvBool                  bKeepAspect;

    NvxOverlay              oOverlay;
    NvBool                  bUseFlip;
    NvBool                  bTurnOffWinA;

    NvBool                  bSmartDimmerEnable;

    OMX_S32 nAVSyncOffset;
    OMX_U32 nFramesSinceLastDrop;

    OMX_BOOL bNoAVSync;
    OMX_BOOL bProfile;
    char     ProfileFileName[PROFILE_FILE_NAME_LENGTH];
    OMX_BOOL bShowJitter;
    OMX_U32  nFrameDropFreq;
    OMX_TICKS nLastTimeStamp;
    OMX_U64  nLastDisplayTime;
    OMX_BOOL bNoAVSyncOverride;

    NvU64    llStartTS[6000];
    NvU64    llPostCSC[6000];
    NvU64    llPostBlt[6000];
    NvU32    nProfCount;
    NvU32    nPos;

    OMX_BOOL bSanity;
    OMX_U32  nAvgFPS;
    OMX_U32  nTotFrameDrops;

    NvxJitterTool *pDeliveryJitter;

    OMX_BOOL nonStarvationPosted;
    OMX_S32 nFramesSinceStarved;
    OMX_BOOL bDisableRendering;

    OMX_U32  nDBWriteFrame; // which double buffer frame to write to
    OMX_U32  nDBDrawFrame;  // which double buffer frame to render
    OMX_U32  nScreenRotation;
    NVX_VIDREND_METRIC videoMetrics;

    // To capture video frame to raw buffer
    NvOsSemaphoreHandle semCapture;
    NvBool              bNeedCapture;
    OMX_U8             *pCaptureBuffer;

    // Support non-standard OMX color formatted input.
    // Boosts performance when not NVIDIA/OMX-tunneled
    NvBool                  bSourceFormatTiled;

    OMX_S32 xScale;
    OMX_BOOL bSetLate;

    OMX_BOOL bSingleFrameModeIsEnabled;
    OMX_BOOL bDisplayNextFrame;

    OMX_U32 nFramesSinceFlush;
    OMX_BOOL bForce2DOverride;

    OMX_U32 StereoRendModeFlag;

    OMX_BOOL bCameraPreviewMode;

    OMX_BOOL bExternalAVSync;
    OMX_PTR pAVSyncAppData;
    NVX_AVSYNC_CALLBACK pAVSyncCallBack;
    NvUPtr pPassedInANW;
    OMX_BOOL bUsedPassedInANW;

    OMX_BOOL bSizeSetDuringAlloc;
    NVX_CONFIG_PROFILE oRenderProf;

    NvUPtr pPassedInAndroidCameraPreview;
    OMX_BOOL bUsedPassedInAndroidCameraPreview;

#ifdef BUILD_GOOGLETV
    NVX_VIDREND_METRIC renderFPSInfoMetrics;
    OMX_U32 nPreviousFPSInfoTime;

    NvxPort *pSchedulerClockPort;
    OMX_BOOL bCheckedSchedulerClockPort;
#endif

    NvU32  overlayIndex;
    NvU32  overlayDepth;

    OMX_BOOL bAllowSecondaryWindow;
} SNvxVideoRenderData;

#define NVX_VIDREND_VIDEO_PORT 0
#define NVX_VIDREND_CLOCK_PORT 1

#ifdef BUILD_GOOGLETV
#define NVX_SCHEDULER_CLOCK_PORT 2
#define OMX_NV_SCHEDULER_OMX_COMP "OMX.Nvidia.vid.Scheduler"
#endif

// NVIDIA Video Renderer and Overlay Renderer String Table
typedef struct NVX_VIDREND_STR_PAIR
{
    OMX_COLOR_FORMATTYPE eColorFormat;
    OMX_STRING szName;
} NVX_VIDREND_STR_PAIR;

static NVX_VIDREND_STR_PAIR s_pVideoRendererNameTable[] =
{
    { OMX_COLOR_Format16bitRGB565,        "OMX.Nvidia.video.render" },
    { OMX_COLOR_Format32bitARGB8888,      "OMX.Nvidia.video.render.argb8888" }
};

static NVX_VIDREND_STR_PAIR s_pOverlayRendererNameTable[] =
{
    { OMX_COLOR_Format16bitRGB565,   "OMX.Nvidia.std.iv_renderer.overlay.rgb565" },
    { OMX_COLOR_Format32bitARGB8888, "OMX.Nvidia.render.overlay.argb8888" },
    { OMX_COLOR_FormatYUV420Planar,  "OMX.Nvidia.std.iv_renderer.overlay.yuv420" }
};

static NVX_VIDREND_STR_PAIR s_pHdmiRendererNameTable[] =
{
    { OMX_COLOR_Format32bitARGB8888, "OMX.Nvidia.render.hdmi.overlay.argb8888" },
    { OMX_COLOR_FormatYUV420Planar,  "OMX.Nvidia.render.hdmi.overlay.yuv420" }
};

static NVX_VIDREND_STR_PAIR s_pLvdsRendererNameTable[] =
{
    { OMX_COLOR_Format32bitARGB8888, "OMX.Nvidia.render.lvds.overlay.argb8888" },
    { OMX_COLOR_FormatYUV420Planar,  "OMX.Nvidia.render.lvds.overlay.yuv420" }
};

static NVX_VIDREND_STR_PAIR s_pCrtRendererNameTable[] =
{
    { OMX_COLOR_Format32bitARGB8888, "OMX.Nvidia.render.crt.overlay.argb8888" },
    { OMX_COLOR_FormatYUV420Planar,  "OMX.Nvidia.render.crt.overlay.yuv420" }
};

static NVX_VIDREND_STR_PAIR s_pTvoutRendererNameTable[] =
{
    { OMX_COLOR_FormatYUV420Planar,  "OMX.Nvidia.render.tvout.overlay.yuv420" }
};

static NVX_VIDREND_STR_PAIR s_pSecondaryRendererNameTable[] =
{
    { OMX_COLOR_FormatYUV420Planar,  "OMX.Nvidia.render.secondary.overlay.yuv420" }
};

/* =========================================================================
 *                     P R O T O T Y P E S
 * ========================================================================= */

/* Core functions */
static OMX_ERRORTYPE NvxVideoRenderDeInit(OMX_IN NvxComponent *hComponent);
static OMX_ERRORTYPE NvxVideoRenderGetConfig(OMX_IN NvxComponent* pNvComp, OMX_IN OMX_INDEXTYPE nIndex, OMX_INOUT OMX_PTR pComponentConfigStructure);
static OMX_ERRORTYPE NvxVideoRenderWorkerFunction(OMX_IN NvxComponent *hComponent, OMX_IN OMX_BOOL bAllPortsReady, OMX_OUT OMX_BOOL *pbMoreWork, OMX_INOUT NvxTimeMs* puMaxMsecToNextCall);
static OMX_ERRORTYPE NvxVideoRenderAcquireResources(OMX_IN NvxComponent *hComponent);
static OMX_ERRORTYPE NvxVideoRenderReleaseResources(OMX_IN NvxComponent *hComponent);
static OMX_ERRORTYPE NvxVideoRenderFlush(OMX_IN NvxComponent *hComponent, OMX_U32 nPort);
static OMX_ERRORTYPE NvxVideoRenderChangeState(OMX_IN NvxComponent *hComponent,OMX_IN OMX_STATETYPE hNewState);
static OMX_ERRORTYPE NvxVideoRenderPreChangeState(OMX_IN NvxComponent *hComponent,OMX_IN OMX_STATETYPE hNewState);
static OMX_ERRORTYPE NvxVideoRenderPreCheckChangeState(OMX_IN NvxComponent *hComponent,OMX_IN OMX_STATETYPE hNewState);
static OMX_ERRORTYPE NvxVideoRenderPortEvent(NvxComponent *pNvComp, OMX_U32 nPort, OMX_U32 uEventType);

/* Init functions */
OMX_ERRORTYPE NvxVideoOverlayRGB565Init(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxVideoOverlayARGB8888Init(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxVideoOverlayYUV420Init(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxVideoHdmiYUV420Init(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxVideoLvdsYUV420Init(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxVideoCrtYUV420Init(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxVideoHdmiARGBInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxVideoLvdsARGBInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxVideoCrtARGBInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxVideoTvoutYUV420Init(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxVideoSecondaryYUV420Init(OMX_IN  OMX_HANDLETYPE hComponent);
static OMX_ERRORTYPE NvxVideoRenderBaseInit(OMX_IN OMX_HANDLETYPE hComponent, OMX_COLOR_FORMATTYPE eColorFormat, ENvxVidRendType oType, OMX_BOOL bForceColorFormat );

// Component setup functions
static OMX_STRING NvxVideoRenderGetComponentName( OMX_COLOR_FORMATTYPE eColorFormat, ENvxVidRendType oType);
static OMX_ERRORTYPE NvxVideoRenderSetComponentRoles( NvxComponent *pNvComp, OMX_COLOR_FORMATTYPE eColorFormat);
static void NvxVideoRenderRegisterFunctions( NvxComponent *pNvComp, OMX_COLOR_FORMATTYPE eColorFormat);


/* Internal functions */
static OMX_ERRORTYPE NvxVideoRenderSetup(SNvxVideoRenderData * pNvxVideoRender);
static OMX_ERRORTYPE NvxVideoRenderOpen( SNvxVideoRenderData * pNvxVideoRender );
static OMX_ERRORTYPE NvxVideoRenderClose( SNvxVideoRenderData * pNvxVideoRender );
static OMX_ERRORTYPE NvxVideoRenderDisplayCurrentBuffer(OMX_IN NvxComponent *hComponent, SNvxVideoRenderData *pNvxVideoRender);

static OMX_ERRORTYPE NvxVideoRenderPrepareSourceSurface(OMX_IN NvxComponent *hComponent, SNvxVideoRenderData * pNvxVideoRender);
static OMX_ERRORTYPE NvxVideoRenderDrawToSurface(SNvxVideoRenderData * pNvxVideoRender);
static OMX_ERRORTYPE NvxVideoRenderFlush2D(SNvxVideoRenderData *pNvxVideoRender);
static OMX_ERRORTYPE NvxVideoRendererTestCropRect(SNvxVideoRenderData * pNvxVideoRender);
static OMX_ERRORTYPE NvxVideoRenderCopyFrame(SNvxVideoRenderData * pNvxVideoRender);

static NvDdk2dSurfaceType NvxMapSurfaceType (NvMMSurfaceDescriptor* surf);

#ifdef BUILD_GOOGLETV
static OMX_BOOL NvxGetSchedulerClockPort(OMX_IN NvxComponent *hComponent);
#endif

/* =========================================================================
 *                     F U N C T I O N S
 * ========================================================================= */

static void NvxVideoRenderSetStarvation(SNvxVideoRenderData *pData,
                                        OMX_BOOL bStarved)
{
    if (pData->dfsClient == 0)
        return;

    if (bStarved)
    {
        NvRmPowerStarvationHint(pData->hRmDevice, NvRmDfsClockId_Cpu,
                                pData->dfsClient, NV_TRUE);
        NvRmPowerStarvationHint(pData->hRmDevice, NvRmDfsClockId_Apb,
                                pData->dfsClient, NV_TRUE);
        NvRmPowerStarvationHint(pData->hRmDevice, NvRmDfsClockId_Vpipe,
                                pData->dfsClient, NV_TRUE);
        NvRmPowerStarvationHint(pData->hRmDevice, NvRmDfsClockId_Avp,
                                pData->dfsClient, NV_TRUE);
        pData->nonStarvationPosted = OMX_FALSE;
    }
    else
    {
        NvRmPowerStarvationHint(pData->hRmDevice, NvRmDfsClockId_Cpu,
                                pData->dfsClient, NV_FALSE);
        NvRmPowerStarvationHint(pData->hRmDevice, NvRmDfsClockId_Apb,
                                pData->dfsClient, NV_FALSE);
        NvRmPowerStarvationHint(pData->hRmDevice, NvRmDfsClockId_Vpipe,
                                pData->dfsClient, NV_FALSE);
        NvRmPowerStarvationHint(pData->hRmDevice, NvRmDfsClockId_Avp,
                                pData->dfsClient, NV_FALSE);
        pData->nonStarvationPosted = OMX_TRUE;
    }
}

static void NvxVideoRenderSetBusy(SNvxVideoRenderData *pData, OMX_U32 nMS)
{
    if (pData->dfsClient == 0)
        return;

    NvRmPowerBusyHint(pData->hRmDevice, NvRmDfsClockId_Cpu,
                            pData->dfsClient, nMS, NvRmFreqMaximum);
    NvRmPowerBusyHint(pData->hRmDevice, NvRmDfsClockId_Apb,
                            pData->dfsClient, nMS, NvRmFreqMaximum);
    NvRmPowerBusyHint(pData->hRmDevice, NvRmDfsClockId_Vpipe,
                            pData->dfsClient, nMS, NvRmFreqMaximum);
}

/**
 * @brief     Base initialization function : variants include different
 *            surface types, color formats, and distinction in allocated vs visible formats
 * @remarks   Base initialization function
 * @returns   OMX_ERRORTYPE value
 * */
static OMX_ERRORTYPE NvxVideoRenderBaseInit(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_COLOR_FORMATTYPE eColorFormat,
    ENvxVidRendType oType, OMX_BOOL bForceColorFormat )
{
    NvxComponent *pNvComp     = NULL;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;
    SNvxVideoRenderData * pNvxVideoRender = NULL;

    eError = NvxComponentCreate(hComponent, 2, &pNvComp);
    if (OMX_ErrorNone != eError)
        return eError;

    pNvComp->DeInit = NvxVideoRenderDeInit;
    pNvComp->ChangeState = NvxVideoRenderChangeState;
    pNvComp->PreChangeState = NvxVideoRenderPreChangeState;
    pNvComp->PreCheckChangeState = NvxVideoRenderPreCheckChangeState;
    pNvComp->Flush = NvxVideoRenderFlush;

    pNvComp->pComponentData = NvOsAlloc(sizeof(SNvxVideoRenderData));
    if (!pNvComp->pComponentData)
    {
        return OMX_ErrorInsufficientResources;
    }

    pNvxVideoRender = (SNvxVideoRenderData *)pNvComp->pComponentData;
    NvOsMemset(pNvComp->pComponentData, 0, sizeof(SNvxVideoRenderData));

    pNvxVideoRender->oRenderType = oType;

    pNvxVideoRender->StereoRendModeFlag  = OMX_STEREORENDERING_OFF;
    eError = NvxVideoRenderSetup((SNvxVideoRenderData *)pNvComp->pComponentData);

    /* Register name */
    pNvComp->pComponentName = NvxVideoRenderGetComponentName(eColorFormat, oType);
    if (!pNvComp->pComponentName)
        pNvComp->pComponentName = "OMX.Nvidia.video.render";

    /* Register function pointers */
    NvxVideoRenderRegisterFunctions(pNvComp, eColorFormat);

    // Set component roles
    NvxVideoRenderSetComponentRoles(pNvComp, eColorFormat);

    if (bForceColorFormat)
    {
        NvxPortInitVideo( &pNvComp->pPorts[NVX_VIDREND_VIDEO_PORT], OMX_DirInput,  2, 1024, OMX_VIDEO_CodingUnused);
        pNvComp->pPorts[NVX_VIDREND_VIDEO_PORT].oPortDef.format.video.eColorFormat = eColorFormat;
    }
    else
    {
        NvxPortInitVideo( &pNvComp->pPorts[NVX_VIDREND_VIDEO_PORT], OMX_DirInput,  2, 1024, OMX_VIDEO_CodingAutoDetect);
    }

    // TO DO: Rename GFSURFACES to NVSURFACES
    pNvComp->pPorts[NVX_VIDREND_VIDEO_PORT].eNvidiaTunnelTransaction = ENVX_TRANSACTION_GFSURFACES;

    NvxPortInitOther(&pNvComp->pPorts[NVX_VIDREND_CLOCK_PORT], OMX_DirInput, 4,
                     sizeof(OMX_TIME_MEDIATIMETYPE), OMX_OTHER_FormatTime);
    pNvComp->pPorts[NVX_VIDREND_CLOCK_PORT].eNvidiaTunnelTransaction = ENVX_TRANSACTION_CLOCK;
    pNvComp->pPorts[NVX_VIDREND_CLOCK_PORT].oPortDef.bEnabled = OMX_FALSE;

    pNvxVideoRender->pInPort = &pNvComp->pPorts[NVX_VIDREND_VIDEO_PORT];

    pNvxVideoRender->bSourceFormatTiled = NV_FALSE;

#ifdef BUILD_GOOGLETV
    NvOsMemset(&(pNvxVideoRender->videoMetrics), 0, (sizeof(NVX_VIDREND_METRIC)));
    NvOsMemset(&(pNvxVideoRender->renderFPSInfoMetrics), 0, (sizeof(NVX_VIDREND_METRIC)));
    pNvxVideoRender->nPreviousFPSInfoTime = 0;
#endif

    pNvxVideoRender->overlayIndex = DEFAULT_TEGRA_OVERLAY_INDEX;
    pNvxVideoRender->overlayDepth = DEFAULT_TEGRA_OVERLAY_DEPTH;

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVideoRenderBaseInit\n"));
    return eError;
}

/**
 * @brief     individualized initialization functions, these are the
 *            functions registered in the OMX component list
 * @remarks   initialization functions
 * @returns   OMX_ERRORTYPE value
 * */

OMX_ERRORTYPE NvxVideoOverlayRGB565Init(OMX_IN  OMX_HANDLETYPE hComponent)
{
    return NvxVideoRenderBaseInit( hComponent,
        OMX_COLOR_Format16bitRGB565, Rend_TypeOverlay, OMX_FALSE );
}

OMX_ERRORTYPE NvxVideoOverlayYUV420Init(OMX_IN  OMX_HANDLETYPE hComponent)
{
    return NvxVideoRenderBaseInit( hComponent,
        OMX_COLOR_FormatYUV420Planar, Rend_TypeOverlay, OMX_FALSE );
}

OMX_ERRORTYPE NvxVideoOverlayARGB8888Init(OMX_IN  OMX_HANDLETYPE hComponent)
{
    return NvxVideoRenderBaseInit( hComponent,
        OMX_COLOR_Format32bitARGB8888, Rend_TypeOverlay, OMX_FALSE );
}

OMX_ERRORTYPE NvxVideoHdmiYUV420Init(OMX_IN  OMX_HANDLETYPE hComponent)
{
    return NvxVideoRenderBaseInit( hComponent,
        OMX_COLOR_FormatYUV420Planar, Rend_TypeHDMI, OMX_FALSE );
}

OMX_ERRORTYPE NvxVideoLvdsYUV420Init(OMX_IN  OMX_HANDLETYPE hComponent)
{
    return NvxVideoRenderBaseInit( hComponent,
        OMX_COLOR_FormatYUV420Planar, Rend_TypeLVDS, OMX_FALSE );
}

OMX_ERRORTYPE NvxVideoCrtYUV420Init(OMX_IN  OMX_HANDLETYPE hComponent)
{
    return NvxVideoRenderBaseInit( hComponent,
        OMX_COLOR_FormatYUV420Planar, Rend_TypeCRT, OMX_FALSE );
}

OMX_ERRORTYPE NvxVideoHdmiARGBInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    return NvxVideoRenderBaseInit( hComponent,
        OMX_COLOR_Format32bitARGB8888, Rend_TypeHDMI, OMX_FALSE );
}

OMX_ERRORTYPE NvxVideoLvdsARGBInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    return NvxVideoRenderBaseInit( hComponent,
        OMX_COLOR_Format32bitARGB8888, Rend_TypeLVDS, OMX_FALSE );
}

OMX_ERRORTYPE NvxVideoCrtARGBInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    return NvxVideoRenderBaseInit( hComponent,
        OMX_COLOR_Format32bitARGB8888, Rend_TypeCRT, OMX_FALSE );
}

OMX_ERRORTYPE NvxVideoTvoutYUV420Init(OMX_IN  OMX_HANDLETYPE hComponent)
{
    return NvxVideoRenderBaseInit( hComponent,
        OMX_COLOR_FormatYUV420Planar, Rend_TypeTVout, OMX_FALSE );
}

OMX_ERRORTYPE NvxVideoSecondaryYUV420Init(OMX_IN  OMX_HANDLETYPE hComponent)
{
    return NvxVideoRenderBaseInit( hComponent,
        OMX_COLOR_FormatYUV420Planar, Rend_TypeSecondary, OMX_FALSE );
}

static OMX_ERRORTYPE NvxVideoRenderDeInit(NvxComponent *pNvComp)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    //SNvxVideoRenderData * pNvxVideoRender =pNvComp->pComponentData;
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVideoRenderDeInit\n"));


    if (pNvComp->pComponentData)
    {
        NvOsFree(pNvComp->pComponentData);
        pNvComp->pComponentData = NULL;
    }
    return eError;
}

static OMX_BOOL NvxIsDoubleBuffering(SNvxVideoRenderData * pNvxVideoRender)
{
    // Current conditions for using double buffering:
    // 1. OS screen rotation is zero
    // 2. we have allocated an intermediate surface (if direct, simple update fast enough)
    // 3. we're doing a flip (if we're doing blt, we'd go down a different path)
    // 4. rotation == 0 . rotation != 0 means we're going down a blt path
    return (OMX_BOOL) (pNvxVideoRender->bInternalSurfAlloced &&
            pNvxVideoRender->bUseFlip &&
            pNvxVideoRender->nRotation == 0 &&
            pNvxVideoRender->nScreenRotation == 0);
}

static OMX_ERRORTYPE NvxVideoCaptureRawFrame(SNvxVideoRenderData* pNvxVideoRender, NvxPort* pVideoPort)
{
    NvxSurface *pSurface;
    NvU32 curlen, length = 0;
    int index;
    OMX_U32 nWidth = pVideoPort->oPortDef.format.video.nFrameWidth;
    OMX_U32 nHeight = pVideoPort->oPortDef.format.video.nFrameHeight;

    if (NvxIsDoubleBuffering(pNvxVideoRender) && (pNvxVideoRender->nDBDrawFrame == 0))
    {
        pSurface = pNvxVideoRender->oOverlay.pSurface;
    }
    else
    {
        pSurface = pNvxVideoRender->pSrcSurface;
    }

    if (!pSurface)
        return OMX_ErrorNotReady;

    for (index = 0; index < pSurface->SurfaceCount; index++)
    {
        if (pSurface->Surfaces[index].hMem )
        {
            if (pSurface->Surfaces[index].ColorFormat == NvColorFormat_A8B8G8R8)
            {
                curlen = nWidth * nHeight;
                curlen *= 4;
                NvRmSurfaceRead(
                    &(pSurface->Surfaces[index]),
                    0,
                    0,
                    nWidth,
                    nHeight,
                    pNvxVideoRender->pCaptureBuffer + length);
            }
            else if (pSurface->Surfaces[index].ColorFormat == NvColorFormat_Y8)
            {
                curlen = nWidth * nHeight;
                NvRmSurfaceRead(
                    &(pSurface->Surfaces[index]),
                    0,
                    0,
                    nWidth,
                    nHeight,
                    pNvxVideoRender->pCaptureBuffer + length);
            }
            else
            {
                curlen = (nWidth/2) * (nHeight/2);
                NvRmSurfaceRead(
                    &(pSurface->Surfaces[index]),
                    0,
                    0,
                    nWidth/2,
                    nHeight/2,
                    pNvxVideoRender->pCaptureBuffer + length);
            }

            length += curlen;
        }
    }

    return OMX_ErrorNone;
}


/**
 * @brief     OMX function for acquiring attributes
 * @remarks   OMX function for acquiring attributes
 * @returns   OMX_ERRORTYPE value
 * */
static OMX_ERRORTYPE NvxVideoRenderGetConfig(
                                             OMX_IN NvxComponent* pNvComp,
                                             OMX_IN OMX_INDEXTYPE nIndex,
                                             OMX_INOUT OMX_PTR pComponentConfigStructure)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    SNvxVideoRenderData *pNvxVideoRender = NULL;
    NvxPort* pVideoPort = NULL;

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVideoRenderGetConfig\n"));

    pNvxVideoRender = (SNvxVideoRenderData *)pNvComp->pComponentData;
    pVideoPort = &pNvComp->pPorts[NVX_VIDREND_VIDEO_PORT];

    switch (nIndex)
    {
        case NVX_IndexConfigProfile:
        {
            NVX_CONFIG_PROFILE *pProf = (NVX_CONFIG_PROFILE *)pComponentConfigStructure;
            if (pNvxVideoRender->bSanity)
            {
                NvU64 maxTime = 0, minTime = (NvU64)-1;
                OMX_U32 i;

                for (i = 0; i < pNvxVideoRender->nProfCount; i++)
                {
                    if (pNvxVideoRender->llPostBlt[i] < minTime)
                        minTime = pNvxVideoRender->llPostBlt[i];
                    if (pNvxVideoRender->llPostBlt[i] > maxTime)
                        maxTime = pNvxVideoRender->llPostBlt[i];
                }

                pNvxVideoRender->nAvgFPS = (OMX_U32)((1000 *
                                         (NvU64)pNvxVideoRender->nProfCount) /
                                         ((maxTime - minTime) / 1000));
            }

            pProf->nAvgFPS = pNvxVideoRender->nAvgFPS;
            pProf->nTotFrameDrops = pNvxVideoRender->nTotFrameDrops;

            break;
        }
        case OMX_IndexConfigCommonOutputPosition:
            NvOsMemcpy( pComponentConfigStructure, &(pNvxVideoRender->oPosition), sizeof(OMX_CONFIG_POINTTYPE));
            break;
        case NVX_IndexConfigDestinationSurface:
            break;
        case OMX_IndexConfigCommonRotate:
            {
                OMX_CONFIG_ROTATIONTYPE *pRotateType;
                pRotateType = (OMX_CONFIG_ROTATIONTYPE *)pComponentConfigStructure;
                pRotateType->nRotation = pNvxVideoRender->nSetRotation;
            }
            break;
        case OMX_IndexConfigCommonMirror:
            {
                OMX_CONFIG_MIRRORTYPE *pMirrorType;
                pMirrorType = (OMX_CONFIG_MIRRORTYPE *)pComponentConfigStructure;
                pMirrorType->eMirror = pNvxVideoRender->eMirror;
            }
            break;
        case OMX_IndexConfigCommonColorKey:
            {
                OMX_CONFIG_COLORKEYTYPE *pColorKey;
                pColorKey = (OMX_CONFIG_COLORKEYTYPE *)pComponentConfigStructure;
                pColorKey->nARGBColor = pNvxVideoRender->oColorKey.nARGBColor;
                pColorKey->nARGBMask  = pNvxVideoRender->oColorKey.nARGBMask;
            }
            break;
        case OMX_IndexConfigCommonPlaneBlend:
            {
                OMX_CONFIG_PLANEBLENDTYPE *pPlaneBlendType;
                pPlaneBlendType = (OMX_CONFIG_PLANEBLENDTYPE *)pComponentConfigStructure;
                pPlaneBlendType->nDepth = pNvxVideoRender->overlayDepth;
                /* TODO : set alpha */
            }
            break;
        case OMX_IndexConfigCommonColorBlend:
            {
                OMX_CONFIG_COLORBLENDTYPE *pColorBlendType;
                pColorBlendType = (OMX_CONFIG_COLORBLENDTYPE *) pComponentConfigStructure;
                pColorBlendType->nRGBAlphaConstant = (pNvxVideoRender->nUniformAlpha & 0xFF)<<24;
                pColorBlendType->eColorBlend = pNvxVideoRender->eColorBlend;
            }
            break;
        case NVX_IndexConfigRenderType:
            {
                NVX_CONFIG_RENDERTARGETTYPE *pRendType =
                    (NVX_CONFIG_RENDERTARGETTYPE *)pComponentConfigStructure;
                pRendType->eType = pNvxVideoRender->eRenderMode;
            }
            break;
        case OMX_IndexConfigCommonOutputSize:
            {
                OMX_FRAMESIZETYPE *pFrameSizeType = (OMX_FRAMESIZETYPE *)pComponentConfigStructure;
                NvRect *pRect = &pNvxVideoRender->rtDestRect;
                pFrameSizeType->nWidth = pRect->right - pRect->left + 1;
                pFrameSizeType->nHeight = pRect->bottom - pRect->top + 1;
            }
            break;
        case OMX_IndexConfigCommonInputCrop:
            {
                OMX_CONFIG_RECTTYPE *pCrop = (OMX_CONFIG_RECTTYPE *)pComponentConfigStructure;
                NvRect *pRect = &pNvxVideoRender->rtUserCropRect;
                if (pNvxVideoRender->bUserCropRectSet)
                {
                    pCrop->nLeft = pRect->left;
                    pCrop->nTop = pRect->top;
                    pCrop->nWidth = pRect->right - pRect->left + 1;
                    pCrop->nHeight = pRect->bottom - pRect->top + 1;
                }
                else
                {
                    pCrop->nLeft = pCrop->nTop = 0;
                    pCrop->nWidth = pVideoPort->oPortDef.format.video.nFrameWidth;
                    pCrop->nHeight = pVideoPort->oPortDef.format.video.nFrameHeight;
                }
            }
            break;
        case NVX_IndexConfigKeepAspect:
            {
                NVX_CONFIG_KEEPASPECT *pAspect = (NVX_CONFIG_KEEPASPECT *)pComponentConfigStructure;
                pAspect->bKeepAspect = pNvxVideoRender->bKeepAspect;
            }
            break;
        case NVX_IndexConfigCaptureRawFrame:
            {
                NVX_CONFIG_CAPTURERAWFRAME *pRawFrame = (NVX_CONFIG_CAPTURERAWFRAME *)pComponentConfigStructure;
                OMX_U32 nWidth = pVideoPort->oPortDef.format.video.nFrameWidth;
                OMX_U32 nHeight = pVideoPort->oPortDef.format.video.nFrameHeight;
                OMX_COLOR_FORMATTYPE eClrFormat;
                OMX_U32 nOutBufSize = 0;

                eClrFormat = pNvxVideoRender->eRequestedColorFormat;

                if (eClrFormat == NvColorFormat_A8R8G8B8)
                {
                    nOutBufSize = nWidth*nHeight*4;
                }
                else if (eClrFormat == NvColorFormat_Y8)
                {
                    nOutBufSize = nWidth*nHeight*3/2;
                }
                else
                {
                    nOutBufSize = 0;
                    pRawFrame->nBufferSize = nOutBufSize;
                    eError = OMX_ErrorUnsupportedSetting;
                    break;
                }

                if ((pRawFrame->pBuffer == NULL) || (pRawFrame->nBufferSize < nOutBufSize))
                {
                   eError = OMX_ErrorBadParameter;
                   pRawFrame->nBufferSize = nOutBufSize;
                   break;
                }

                pNvxVideoRender->pCaptureBuffer = pRawFrame->pBuffer;

                if (pNvxVideoRender->bPaused)
                {
                    eError = NvxVideoCaptureRawFrame(pNvxVideoRender, pVideoPort);
                }
                else
                {
                    pNvxVideoRender->bNeedCapture = NV_TRUE;
                    if (NvError_Timeout == NvOsSemaphoreWaitTimeout(pNvxVideoRender->semCapture, 1000))
                    {
                        pNvxVideoRender->bNeedCapture = NV_FALSE;
                        eError = OMX_ErrorTimeout;
                    }
                }
            }
            break;
        case NVX_IndexConfigCustomColorFormt:
            {
                NVX_CONFIG_CUSTOMCOLORFORMAT *pFormat = (NVX_CONFIG_CUSTOMCOLORFORMAT *)pComponentConfigStructure;
                pFormat->bTiled = (OMX_BOOL)pNvxVideoRender->bSourceFormatTiled;
            }
            break;
        case NVX_IndexConfigNumRenderedFrames:
            {
                NVX_CONFIG_NUMRENDEREDFRAMES *pNumFrames = (NVX_CONFIG_NUMRENDEREDFRAMES *)pComponentConfigStructure;
                pNumFrames->nFrames = pNvxVideoRender->nFramesSinceFlush;
            }
            break;
        case NVX_IndexConfigRenderHintMixedFrames:
            {
                NVX_CONFIG_RENDERHINTMIXEDFRAMES *pMixedFrames = (NVX_CONFIG_RENDERHINTMIXEDFRAMES *)pComponentConfigStructure;
                pMixedFrames->bMixedFrames = pNvxVideoRender->bForce2DOverride;
                break;
            }
        case NVX_IndexConfigStereoRendMode:
            {
                OMX_CONFIG_STEREORENDMODETYPE *pStereoRendMode =
                (OMX_CONFIG_STEREORENDMODETYPE *)pComponentConfigStructure;
                switch(pNvxVideoRender->StereoRendModeFlag)
                {
                    case OMX_STEREORENDERING_HOR_STITCHED:
                        pStereoRendMode->eType = OMX_STEREORENDERING_HOR_STITCHED;
                        break;
                    default:
                        pStereoRendMode->eType = OMX_STEREORENDERING_OFF;
                        break;
                }
                break;
            }
        case NVX_IndexConfigRenderHintCameraPreview:
            {
                NVX_CONFIG_RENDERHINTCAMERAPREVIEW *pCameraPreview = (NVX_CONFIG_RENDERHINTCAMERAPREVIEW *)pComponentConfigStructure;
                pCameraPreview->bCameraPreviewMode = pNvxVideoRender->bCameraPreviewMode;
                break;
            }

        case NVX_IndexConfigOverlayIndex:
            {
                NVX_CONFIG_OVERLAYINDEX *pOverlayIndex = (NVX_CONFIG_OVERLAYINDEX *)pComponentConfigStructure;
                pOverlayIndex->index = pNvxVideoRender->overlayIndex;
                break;
            }

        case NVX_IndexConfigAllowSecondaryWindow:
            {
                NVX_CONFIG_ALLOWSECONDARYWINDOW *pAllowSecondaryWindow = (NVX_CONFIG_ALLOWSECONDARYWINDOW *)pComponentConfigStructure;
                pAllowSecondaryWindow->bAllow = pNvxVideoRender->bAllowSecondaryWindow;
            }

        default:
            return NvxComponentBaseGetConfig( pNvComp, nIndex, pComponentConfigStructure);
    }
    return eError;
}

/**
 * @brief     OMX function for setting attributes
 * @remarks   OMX function for setting attributes
 * @returns   OMX_ERRORTYPE value
 * */
/* FIXME: Make this static once EGL changes */
OMX_ERRORTYPE NvxVideoRenderSetConfig(OMX_IN NvxComponent* pNvComp,
                                      OMX_IN OMX_INDEXTYPE nIndex,
                                      OMX_IN OMX_PTR pComponentConfigStructure)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    SNvxVideoRenderData *pNvxVideoRender = 0;
    NvxPort* pVideoPort = NULL;

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVideoRenderSetConfig\n"));

    pVideoPort = &pNvComp->pPorts[NVX_VIDREND_VIDEO_PORT];

    pNvxVideoRender = (SNvxVideoRenderData *)pNvComp->pComponentData;
    pNvxVideoRender->pInPort = pVideoPort;

    switch (nIndex)
    {
        case NVX_IndexConfigProfile:
        {
            NVX_CONFIG_PROFILE *pProf = (NVX_CONFIG_PROFILE *)pComponentConfigStructure;
            pNvxVideoRender->bProfile = pProf->bProfile;
            if (pProf->bProfile)
                NvOsStrncpy(pNvxVideoRender->ProfileFileName, pProf->ProfileFileName, NvOsStrlen(pProf->ProfileFileName)+1);
            pNvxVideoRender->bNoAVSync = pProf->bNoAVSync;
            pNvxVideoRender->nAVSyncOffset = pProf->nAVSyncOffset;
            pNvxVideoRender->bDisableRendering = pProf->bDisableRendering;
            pNvxVideoRender->bSanity = pProf->bSanity;
            pNvxVideoRender->bShowJitter = pProf->nNvMMProfile;
            if (pNvxVideoRender->pDeliveryJitter)
                NvxJitterToolSetShow(pNvxVideoRender->pDeliveryJitter,
                                     pProf->nNvMMProfile);

            if (pProf->nFrameDrop > 0)
                pNvxVideoRender->nFrameDropFreq = pProf->nFrameDrop;

            NvOsMemcpy(&pNvxVideoRender->oRenderProf, pProf, sizeof(NVX_CONFIG_PROFILE));

            break;
        }
        case OMX_IndexConfigCommonOutputPosition:
            {
                OMX_CONFIG_POINTTYPE * pPointType = (OMX_CONFIG_POINTTYPE *)pComponentConfigStructure;
                pNvxVideoRender->oPosition.nX = pPointType->nX;
                pNvxVideoRender->oPosition.nY = pPointType->nY;
                pNvxVideoRender->bNeedUpdate = OMX_TRUE;
            }
            break;
        case OMX_IndexConfigCommonRotate:
            {
                OMX_CONFIG_ROTATIONTYPE * pRotateType = (OMX_CONFIG_ROTATIONTYPE *)pComponentConfigStructure;
                OMX_U32 nScreenRotation = NvxGetOsRotation();
                int newRot;

                newRot = pRotateType->nRotation + nScreenRotation;
                if (newRot < 0)
                {
                    newRot *= -1;
                    newRot %= 360;
                    newRot *= -1;
                    newRot += 360;
                }
                newRot %= 360;

                if ((newRot != pNvxVideoRender->nRotation) ||
                    (pNvxVideoRender->nScreenRotation != nScreenRotation))
                {
                    pNvxVideoRender->bNeedUpdate = OMX_TRUE;
                    pNvxVideoRender->nScreenRotation = nScreenRotation;
                }
                pNvxVideoRender->nRotation = newRot;
                pNvxVideoRender->nSetRotation = pRotateType->nRotation;
            }
            break;
        case OMX_IndexConfigCommonMirror:
            {
                OMX_CONFIG_MIRRORTYPE * pMirrorType = (OMX_CONFIG_MIRRORTYPE *)pComponentConfigStructure;
                pNvxVideoRender->eMirror = pMirrorType->eMirror;
                pNvxVideoRender->bNeedUpdate = OMX_TRUE;
            }
            break;
        case OMX_IndexConfigCommonColorKey:
            {
                OMX_CONFIG_COLORKEYTYPE * pColorKey = (OMX_CONFIG_COLORKEYTYPE *)pComponentConfigStructure;
                pNvxVideoRender->oColorKey.nARGBColor = pColorKey->nARGBColor;
                pNvxVideoRender->oColorKey.nARGBMask = pColorKey->nARGBMask;
                pNvxVideoRender->bColorKeySet = (pColorKey->nARGBMask > 0) ? OMX_TRUE : OMX_FALSE;
                pNvxVideoRender->bNeedUpdate = OMX_TRUE;
            }
            break;
        case OMX_IndexConfigCommonPlaneBlend:
            {
                OMX_CONFIG_PLANEBLENDTYPE *pPlaneBlendType = (OMX_CONFIG_PLANEBLENDTYPE *) pComponentConfigStructure;
                pNvxVideoRender->overlayDepth = pPlaneBlendType->nDepth;
                /*TODO: get alpha */
            }
            break;
        case OMX_IndexConfigCommonColorBlend:
            {
                OMX_U32 nAlphaValue;
                OMX_CONFIG_COLORBLENDTYPE *pColorBlendType = (OMX_CONFIG_COLORBLENDTYPE *) pComponentConfigStructure;
                nAlphaValue = (pColorBlendType->nRGBAlphaConstant>>24) & 0xFF;
                pNvxVideoRender->nUniformAlpha = nAlphaValue<<16 | nAlphaValue<<8 | nAlphaValue;
                pNvxVideoRender->eColorBlend = pColorBlendType->eColorBlend;
                pNvxVideoRender->bNeedUpdate = OMX_TRUE;
            }
            break;
        case NVX_IndexConfigRenderType:
            {
                NVX_CONFIG_RENDERTARGETTYPE *pRendType =
                    (NVX_CONFIG_RENDERTARGETTYPE *)pComponentConfigStructure;
                pNvxVideoRender->eRenderMode = pRendType->eType;
                pNvxVideoRender->pDestSurface = (NvxSurface *)pRendType->hWindow;
                if (pNvxVideoRender->pDestSurface)
                {
                    // Renderer should format data to match outgoing surface
                    // color format:
                    pNvxVideoRender->eRequestedColorFormat =
                        pNvxVideoRender->pDestSurface->Surfaces[0].ColorFormat;
                }
            }
            break;
        case OMX_IndexConfigCommonOutputSize:
            {
                OMX_FRAMESIZETYPE *pFrameSizeType = (OMX_FRAMESIZETYPE *)pComponentConfigStructure;
                NvRect *pRect = &pNvxVideoRender->rtDestRect;

                if( !pFrameSizeType->nWidth || !pFrameSizeType->nHeight)
                    return OMX_ErrorBadParameter;

                pRect->right = pRect->left + pFrameSizeType->nWidth - 1;
                pRect->bottom = pRect->top + pFrameSizeType->nHeight - 1;
                pNvxVideoRender->oOutputSize.nWidth = pFrameSizeType->nWidth;
                pNvxVideoRender->oOutputSize.nHeight = pFrameSizeType->nHeight;
                pNvxVideoRender->bNeedUpdate = OMX_TRUE;
                pNvxVideoRender->bOutputSizeSet = OMX_TRUE;
            }
            break;
        case OMX_IndexConfigCommonInputCrop:
            {
                OMX_CONFIG_RECTTYPE *pCrop = (OMX_CONFIG_RECTTYPE *)pComponentConfigStructure;
                NvRect *pRect = &pNvxVideoRender->rtUserCropRect;
                pRect->left = pCrop->nLeft;
                pRect->top = pCrop->nTop;
                pRect->right = pCrop->nWidth + pCrop->nLeft;
                pRect->bottom = pCrop->nHeight + pCrop->nTop;
                pNvxVideoRender->bNeedUpdate = OMX_TRUE;
                pNvxVideoRender->bUserCropRectSet = NV_TRUE;

                // Note: Do crop check here in case we're in the paused
                // state and have a source surface
                NvxVideoRendererTestCropRect(pNvxVideoRender);
            }
            break;
        case NVX_IndexConfigKeepAspect:
            {
                NVX_CONFIG_KEEPASPECT *pAspect = (NVX_CONFIG_KEEPASPECT *)pComponentConfigStructure;
                pNvxVideoRender->bKeepAspect = pAspect->bKeepAspect;
                pNvxVideoRender->bNeedUpdate = OMX_TRUE;
            }
            break;
        case NvxIndexConfigWindowDispOverride:
            {
                // For now, only accept WinA, and only on/off actions
                NVX_CONFIG_WINDOWOVERRIDE *pWinOverride = (NVX_CONFIG_WINDOWOVERRIDE *)pComponentConfigStructure;
                if (pWinOverride->eWindow != NvxWindow_A)
                    return OMX_ErrorBadParameter;

                pNvxVideoRender->bTurnOffWinA =
                    (NvBool) (pWinOverride->eAction == NvxWindowAction_TurnOff);
            }
            break;
        case NVX_IndexConfigCustomColorFormt:
            {
                NVX_CONFIG_CUSTOMCOLORFORMAT *pFormat = (NVX_CONFIG_CUSTOMCOLORFORMAT *)pComponentConfigStructure;
                pNvxVideoRender->bSourceFormatTiled = (NvBool)pFormat->bTiled;
            }
            break;
        case OMX_IndexConfigTimeScale:
        {
            OMX_TIME_CONFIG_SCALETYPE *pScale= (OMX_TIME_CONFIG_SCALETYPE *)pComponentConfigStructure;
            pNvxVideoRender->xScale = pScale->xScale;
            break;
        }
        case NvxIndexConfigSingleFrame:
        {
            NVX_CONFIG_SINGLE_FRAME *pSingleFrame = (NVX_CONFIG_SINGLE_FRAME *)pComponentConfigStructure;
            pNvxVideoRender->bSingleFrameModeIsEnabled = pSingleFrame->bEnableMode;
            pNvxVideoRender->bDisplayNextFrame = pSingleFrame->bEnableMode;
            break;
        }
        case NVX_IndexConfigRenderHintMixedFrames:
        {
            NVX_CONFIG_RENDERHINTMIXEDFRAMES *pMixedFrames = (NVX_CONFIG_RENDERHINTMIXEDFRAMES *)pComponentConfigStructure;
            pNvxVideoRender->bForce2DOverride = pMixedFrames->bMixedFrames;
            pNvxVideoRender->bNeedUpdate = OMX_TRUE;
            break;
        }
        case NVX_IndexConfigExternalOverlay:
        {
            return OMX_ErrorNotImplemented;
        }
        case NVX_IndexConfigAndroidWindow:
        {
            NVX_CONFIG_ANDROIDWINDOW *pExt = (NVX_CONFIG_ANDROIDWINDOW *)pComponentConfigStructure;

            // Free previous surface, if one exists
            if (pNvxVideoRender->oOverlay.pSurface)
            {
                NvxReleaseOverlay(&pNvxVideoRender->oOverlay);
                pNvxVideoRender->pDestSurface = NULL;
            }

            pNvxVideoRender->pPassedInANW = (NvUPtr)(pExt->oAndroidWindowPtr);
            pNvxVideoRender->bUsedPassedInANW = OMX_TRUE;
            break;
        }
        case NVX_IndexConfigCameraPreview:
        {
            NVX_CONFIG_CAMERAPREVIEW *pExt = (NVX_CONFIG_CAMERAPREVIEW *)pComponentConfigStructure;

            // Free previous surface, if one exists
            if (pNvxVideoRender->oOverlay.pSurface)
            {
                NvxReleaseOverlay(&pNvxVideoRender->oOverlay);
                pNvxVideoRender->pDestSurface = NULL;
            }

            pNvxVideoRender->pPassedInAndroidCameraPreview = (NvUPtr)(pExt->oAndroidCameraPreviewPtr);
            pNvxVideoRender->bUsedPassedInAndroidCameraPreview = OMX_TRUE;
            break;
        }
        case NVX_IndexParamAllocateRmSurf:
        {
            NVX_CONFIG_ALLOCATERMSURF *pAllocate = (NVX_CONFIG_ALLOCATERMSURF *)pComponentConfigStructure;
            pVideoPort->bAllocateRmSurf = (OMX_BOOL)(pAllocate->bRmSurf);
            break;
        }
        case NVX_IndexConfigSmartDimmer:
        {
            NVX_CONFIG_SMARTDIMMER *pSmartDimmer = (NVX_CONFIG_SMARTDIMMER *)pComponentConfigStructure;
            pNvxVideoRender->bSmartDimmerEnable = pSmartDimmer->bSmartDimmerEnable;
            break;
        }
        case NVX_IndexConfigExternalAVSync:
        {
            NVX_CONFIG_EXTERNALAVSYNC *pExt = (NVX_CONFIG_EXTERNALAVSYNC *)pComponentConfigStructure;
            pNvxVideoRender->bExternalAVSync = pExt->pAppData ? OMX_TRUE : OMX_FALSE;
            pNvxVideoRender->pAVSyncAppData = pExt->pAppData;
            pNvxVideoRender->pAVSyncCallBack = pExt->AVSyncCallBack;
            break;
        }

        case NVX_IndexConfigVirtualDesktop:
        {
            return OMX_ErrorNotImplemented;
        }
        case NVX_IndexConfigStereoRendMode:
            {
                OMX_CONFIG_STEREORENDMODETYPE *pStereoRendMode =
                (OMX_CONFIG_STEREORENDMODETYPE *)pComponentConfigStructure;
                switch(pStereoRendMode->eType)
                {
                    case OMX_STEREORENDERING_HOR_STITCHED:
                        pNvxVideoRender->StereoRendModeFlag = OMX_STEREORENDERING_HOR_STITCHED;
                        break;
                    default:
                        /* OMX_STEREORENDERING_OFF */
                        pNvxVideoRender->StereoRendModeFlag  = OMX_STEREORENDERING_OFF;
                        break;
                }
                break;
            }
        case NVX_IndexConfigRenderHintCameraPreview:
        {
            NVX_CONFIG_RENDERHINTCAMERAPREVIEW *pCameraPreview = (NVX_CONFIG_RENDERHINTCAMERAPREVIEW *)pComponentConfigStructure;
            pNvxVideoRender->bCameraPreviewMode = pCameraPreview->bCameraPreviewMode;
            break;
        }

        case NVX_IndexConfigOverlayIndex:
        {
            NVX_CONFIG_OVERLAYINDEX *pOverlayIndex = (NVX_CONFIG_OVERLAYINDEX *)pComponentConfigStructure;
            pNvxVideoRender->overlayIndex = pOverlayIndex->index;
            break;
        }

        case NVX_IndexConfigAllowSecondaryWindow:
        {
            NVX_CONFIG_ALLOWSECONDARYWINDOW *pAllowSecondaryWindow = (NVX_CONFIG_ALLOWSECONDARYWINDOW *)pComponentConfigStructure;
            pNvxVideoRender->bAllowSecondaryWindow = pAllowSecondaryWindow->bAllow;
            break;
        }

        default:
            return NvxComponentBaseSetConfig( pNvComp, nIndex, pComponentConfigStructure);
    }

    if ((pNvxVideoRender->bNeedUpdate) &&
        (pNvComp->eState == OMX_StatePause ||
         pNvComp->ePendingState == OMX_StatePause))
    {
        pNvComp->bNeedRunWorker = OMX_TRUE;
    }

    return eError;
}

static OMX_ERRORTYPE NvxVideoRenderSetParameter(OMX_IN NvxComponent *pNvComp, OMX_IN OMX_INDEXTYPE nIndex, OMX_IN OMX_PTR pComponentParameterStructure)
{
    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVideoRenderSetParameter\n"));

    switch(nIndex)
    {
#ifdef BUILD_GOOGLETV
    case OMX_IndexParamPortDefinition:
    {
        SNvxVideoRenderData *pNvxVideoRender = 0;
        NvxPort* pVideoPort = NULL;
        OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;
        OMX_VIDEO_PORTDEFINITIONTYPE *video_def;
        OMX_ERRORTYPE eError;

        // Call base's setParam to chek errors and set the params.
        eError = NvxComponentBaseSetParameter(pNvComp, nIndex,
                                              pComponentParameterStructure);

        if (OMX_ErrorNone != eError)
        {
            return eError;
        }

        // All the error checking for ports etc is already done in base's setParam
        pPortDef = pComponentParameterStructure;
        pVideoPort = &pNvComp->pPorts[pPortDef->nPortIndex];
        pNvxVideoRender = (SNvxVideoRenderData *)pNvComp->pComponentData;
        video_def = &pVideoPort->oPortDef.format.video;

        if (video_def)
        {
            if (video_def->pNativeWindow)
            {
                // Free previous surface, if one exists
                if (pNvxVideoRender->oOverlay.pSurface)
                {
                    NvxReleaseOverlay(&pNvxVideoRender->oOverlay);
                    pNvxVideoRender->pDestSurface = NULL;
                }

                pNvxVideoRender->pPassedInANW = (NvUPtr)(video_def->pNativeWindow);
                pNvxVideoRender->bUsedPassedInANW = OMX_TRUE;
            }
            else
            {
                return OMX_ErrorInsufficientResources;
            }
        }
        else
        {
            return OMX_ErrorInsufficientResources;
        }
        // return OMX_ErrorNone, navtive window can be set using setConfig.
        return OMX_ErrorNone;
    }
#endif
    default:
        return NvxComponentBaseSetParameter(pNvComp, nIndex, pComponentParameterStructure);
    }
}

static void NvxWriteOMXFrameDataToNvSurf(SNvxVideoRenderData * pNvxVideoRender, OMX_BUFFERHEADERTYPE *pInBuffer)
{
    NvBool bSourceTiled = pNvxVideoRender->bSourceFormatTiled;
    NvxSurface *pSurface = pNvxVideoRender->pSrcSurface;
    NvxPort *pInPort = pNvxVideoRender->pInPort;
    OMX_U32 nWidth = pInPort->oPortDef.format.video.nFrameWidth;
    OMX_U32 nHeight = pInPort->oPortDef.format.video.nFrameHeight;
    NvU32 Width[3], Height[3], Div = 1;
    int index = 0;

    if (NvxIsDoubleBuffering(pNvxVideoRender))
    {
        // In these conditions, we will double buffer
        if (pNvxVideoRender->nDBWriteFrame == 0)
        {
            pNvxVideoRender->nDBWriteFrame = 1;
        }
        else
        {
            pSurface = pNvxVideoRender->oOverlay.pSurface;
            pNvxVideoRender->nDBWriteFrame = 0;
        }
    }

    NV_ASSERT(pSurface != NULL);

    for (index = 0; index < pSurface->SurfaceCount; index++)
    {
        Div = ((index > 0)? 2 : 1);
        Width[index] = nWidth/Div;
        Height[index] = nHeight/Div;
    }

    // Not tunneled: Internal surface should exist
    NvxCopyOMXBufferToNvxSurface(pInBuffer,
                                  pSurface,
                                  Width,
                                  Height,
                                  pSurface->SurfaceCount,
                                  bSourceTiled);
}

static void NvxUpdateOverlaySurface(SNvxVideoRenderData * pNvxVideoRender, OMX_BOOL bUpdateDB, OMX_BOOL bWait)
{
    if (pNvxVideoRender->bDisableRendering == OMX_TRUE)
    {
        return;
    }

    if (NvxIsDoubleBuffering(pNvxVideoRender))
    {
        // If we have an intermediate buffer allocated, we'll double buffer
        if (pNvxVideoRender->nDBDrawFrame == 0)
        {
            NvxRenderSurfaceToOverlay(&pNvxVideoRender->oOverlay,
                                  pNvxVideoRender->pSrcSurface,
                                  bWait);
            if (bUpdateDB)
                pNvxVideoRender->nDBDrawFrame = 1;
        }
        else
        {
            NvxRenderSurfaceToOverlay(&pNvxVideoRender->oOverlay,
                                  pNvxVideoRender->oOverlay.pSurface,
                                  bWait);
            if (bUpdateDB)
                pNvxVideoRender->nDBDrawFrame = 0;
        }
    }
    else
    {
        NvxRenderSurfaceToOverlay(&pNvxVideoRender->oOverlay,
                                      pNvxVideoRender->pSrcSurface,
                                      bWait);
    }
}

static void NvxRendererUpdatePosition(SNvxVideoRenderData *pNvxVideoRender,
                                      NvxPort* pVideoPort, NvxComponent *hComponent)
{
    int nSourceWidth  = (int)pVideoPort->oPortDef.format.video.nFrameWidth;
    int nSourceHeight = (int)pVideoPort->oPortDef.format.video.nFrameHeight;
    int nSourceX = 0, nSourceY = 0;
    int nFrameWidth;
    int nFrameHeight;
    float sourceAspect = (float)nSourceWidth / nSourceHeight;
    float destAspect;
    OMX_BOOL bNeedOverlayUpdate = OMX_FALSE;

    if (!pNvxVideoRender->pDestSurface)
        return;

    if (pNvxVideoRender->pSrcSurface &&
        pNvxVideoRender->oOverlay.bNative && pNvxVideoRender->oOverlay.pSurface)
    {
        if (pNvxVideoRender->bForce2DOverride)
        {
            if (!pNvxVideoRender->oOverlay.bForce2D)
            {
                pNvxVideoRender->oOverlay.bForce2D = NV_TRUE;
                bNeedOverlayUpdate = OMX_TRUE;
            }
        }
        else
        {
            // Check src surface against overlay
            NvxSurface *overlay = pNvxVideoRender->oOverlay.pSurface;
            NvxSurface *src = pNvxVideoRender->pSrcSurface;
            OMX_BOOL bSameFormat = OMX_TRUE;

            if (overlay->SurfaceCount != src->SurfaceCount)
                bSameFormat = OMX_FALSE;
            else
            {
                int i = 0;
                for (i = 0; i < overlay->SurfaceCount; i++)
                {
                    if (overlay->Surfaces[i].Width  != src->Surfaces[i].Width ||
                        overlay->Surfaces[i].Height != src->Surfaces[i].Height)
                    {
                        bSameFormat = OMX_FALSE;
                        break;
                    }
                }
            }

            if (!bSameFormat != pNvxVideoRender->oOverlay.bForce2D)
                bNeedOverlayUpdate = OMX_TRUE;
            pNvxVideoRender->oOverlay.bForce2D = !bSameFormat;
        }
    }

    nFrameWidth = pNvxVideoRender->pDestSurface->Surfaces[0].Width;
    nFrameHeight = pNvxVideoRender->pDestSurface->Surfaces[0].Height;
    destAspect = (float)nFrameWidth / nFrameHeight;

    pNvxVideoRender->oOverlay.bKeepAspect = pNvxVideoRender->bKeepAspect;
    pNvxVideoRender->oOverlay.nSetRotation = pNvxVideoRender->nSetRotation;
    pNvxVideoRender->oOverlay.nRotation = pNvxVideoRender->nRotation;

    if (pNvxVideoRender->bCropSet)
    {
        int l, t, r, b;

        l = pNvxVideoRender->rtActualCropRect.left;
        t = pNvxVideoRender->rtActualCropRect.top;
        r = pNvxVideoRender->rtActualCropRect.right;
        b = pNvxVideoRender->rtActualCropRect.bottom;

        if (r > 0  && r <= nSourceWidth &&
            b > 0  && b <= nSourceHeight &&
            l >= 0 && l < r &&
            t >= 0 && t < b)
        {
            pNvxVideoRender->oOverlay.srcX = l;
            pNvxVideoRender->oOverlay.srcY = t;
            pNvxVideoRender->oOverlay.srcW = r - l;
            pNvxVideoRender->oOverlay.srcH = b - t;

            // Limit destination rectangle in this case
            nFrameWidth = pNvxVideoRender->oOverlay.srcW;
            nFrameHeight = pNvxVideoRender->oOverlay.srcH;

            nSourceWidth = r - l;
            nSourceHeight = b - t;
            nSourceX = l;
            nSourceY = t;
            sourceAspect = (float)nSourceWidth / nSourceHeight;
        }
    }

    pNvxVideoRender->bUseFlip = OMX_TRUE;

    // If crop is set, reset overlay
    if (pNvxVideoRender->bUsingWindow)
    {
        void *hWnd = pVideoPort->oPortDef.format.video.pNativeRender;
        NvRect rtWin;

        if (NvxWindowGetDisplayRect(hWnd, &rtWin))
        {
            NvxUpdateOverlay(&rtWin, &pNvxVideoRender->oOverlay);
            bNeedOverlayUpdate = OMX_FALSE;
        }
    }
    else if (pNvxVideoRender->bOutputSizeSet)
    {
        NvRect rtDest;
        rtDest.left = pNvxVideoRender->oPosition.nX;
        rtDest.top = pNvxVideoRender->oPosition.nY;
        rtDest.right = rtDest.left + pNvxVideoRender->oOutputSize.nWidth;
        rtDest.bottom = rtDest.top + pNvxVideoRender->oOutputSize.nHeight;
        if (!pNvxVideoRender->bSizeSetDuringAlloc)
        {
            NvxUpdateOverlay(&rtDest, &pNvxVideoRender->oOverlay);
            pNvxVideoRender->bSizeSetDuringAlloc = OMX_FALSE;
        }
        bNeedOverlayUpdate = OMX_FALSE;
    }
    else
    {
        NvxUpdateOverlay(NULL, &pNvxVideoRender->oOverlay);
        pNvxVideoRender->pDestSurface = pNvxVideoRender->oOverlay.pSurface;
        bNeedOverlayUpdate = OMX_FALSE;
    }

    if (bNeedOverlayUpdate)
        NvxUpdateOverlay(NULL, &pNvxVideoRender->oOverlay);

    if (pNvxVideoRender->pLastFrame)
    {
        if (pNvxVideoRender->bUseFlip)
        {
            OMX_BOOL bWait = !(pNvxVideoRender->bNoAVSync ||
                               pNvxVideoRender->bNoAVSyncOverride);

            NvxUpdateOverlaySurface(pNvxVideoRender, OMX_FALSE, bWait);
        }
        else
        {
            NvxVideoRenderDrawToSurface(pNvxVideoRender);
        }
    }
}

/**
 * @brief     Video Renderer's worker function
 * @remarks   Does the work when component is in Executing state
 * @returns   OMX_ERRORTYPE value
 * */
static OMX_ERRORTYPE NvxVideoRenderWorkerFunction(OMX_IN NvxComponent *hComponent, OMX_BOOL bAllPortsReady, OMX_OUT OMX_BOOL *pbMoreWork,
                                                  OMX_INOUT NvxTimeMs* puMaxMsecToNextCall)
{
    SNvxVideoRenderData *pNvxVideoRender = 0;
    OMX_ERRORTYPE       eError           = OMX_ErrorNone;
    NvxPort* pVideoPort = &hComponent->pPorts[NVX_VIDREND_VIDEO_PORT];
    NvxPort* pClockPort = &hComponent->pPorts[NVX_VIDREND_CLOCK_PORT];
    OMX_BOOL bDisplay = OMX_TRUE;

    NVXTRACE((NVXT_CALLGRAPH, hComponent->eObjectType, "+NvxVideoRenderWorkerFunction\n"));

    *pbMoreWork = bAllPortsReady;

    pNvxVideoRender = (SNvxVideoRenderData *)hComponent->pComponentData;
    if (!pNvxVideoRender)
        return OMX_ErrorBadParameter;

#ifdef BUILD_GOOGLETV
    if (NvxGetSchedulerClockPort(hComponent) && pNvxVideoRender->pSchedulerClockPort)
    {
        pClockPort = pNvxVideoRender->pSchedulerClockPort;

        if (pClockPort && pClockPort->bNvidiaTunneling)
        {
            OMX_HANDLETYPE hClock = pClockPort->hTunnelComponent;
            if (hClock)
            {
                if (NvxCCGetClockRate(hClock) == 0)
                {
                    *pbMoreWork = OMX_FALSE;
                    *puMaxMsecToNextCall = 100;
                    return OMX_ErrorNone;
                }
            }
        }
    }
#endif

    if (pClockPort && pClockPort->pCurrentBufferHdr)
    {
        NvxPortReleaseBuffer(pClockPort, pClockPort->pCurrentBufferHdr);
        if (!hComponent->bNeedRunWorker)
            return OMX_ErrorNone;
    }

    /* Step mode */
    if (pNvxVideoRender->bSingleFrameModeIsEnabled && !pNvxVideoRender->bDisplayNextFrame)
    {
        return OMX_ErrorNone;
    }

    pNvxVideoRender->pInPort = pVideoPort;

    if (!pVideoPort->pCurrentBufferHdr)
    {
        NvxPortGetNextHdr(pVideoPort);

        if (!pVideoPort->pCurrentBufferHdr)
        {
            //*puMaxMsecToNextCall = 5;
            if (!hComponent->bNeedRunWorker)
            {
                return OMX_ErrorNone;
            }
        }
    }

    if ((pNvxVideoRender->bUsedPassedInANW &&
         pNvxVideoRender->pPassedInANW == 0) ||
        (pNvxVideoRender->bUsedPassedInAndroidCameraPreview &&
         pNvxVideoRender->pPassedInAndroidCameraPreview == 0))
    {
        NvxPortReleaseBuffer(pVideoPort, pVideoPort->pCurrentBufferHdr);
        pVideoPort->pCurrentBufferHdr = NULL;
        return OMX_ErrorNone;
    }

    if (pNvxVideoRender->oOverlay.pSurface && pVideoPort->pCurrentBufferHdr)
    {
        NvBool bNvidiaTunneled = (pVideoPort->bNvidiaTunneling &&
                                 pVideoPort->eNvidiaTunnelTransaction == ENVX_TRANSACTION_GFSURFACES)
                                 || (pVideoPort->pCurrentBufferHdr->nFlags & OMX_BUFFERFLAG_NV_BUFFER);

        if (bNvidiaTunneled)
        {
            NvMMBuffer *pInBuf = (NvMMBuffer *)pVideoPort->pCurrentBufferHdr->pBuffer;
            if ((pInBuf->Payload.Surfaces.Surfaces[0].Width != pNvxVideoRender->oOverlay.pSurface->Surfaces[0].Width) ||
                (pInBuf->Payload.Surfaces.Surfaces[0].Height != pNvxVideoRender->oOverlay.pSurface->Surfaces[0].Height))
            {
                pNvxVideoRender->bSizeSet = OMX_FALSE;
            }
        }
    }

    // If port properties have not been set, acquire default settings:
    if (pVideoPort->hTunnelComponent && !pNvxVideoRender->bSizeSet &&
        pVideoPort->pCurrentBufferHdr)
    {
        void *pNativeRender = pVideoPort->oPortDef.format.video.pNativeRender;
        OMX_PARAM_PORTDEFINITIONTYPE oPortDefinition;
        oPortDefinition.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
        oPortDefinition.nVersion.nVersion = NvxComponentSpecVersion.nVersion;
        oPortDefinition.nPortIndex = pVideoPort->nTunnelPort;
        eError = OMX_GetParameter(pVideoPort->hTunnelComponent,
                                  OMX_IndexParamPortDefinition,
                                  &oPortDefinition);
        NvOsMemcpy(&(pVideoPort->oPortDef.format.video),
                 &(oPortDefinition.format.video),
                 sizeof(OMX_IMAGE_PORTDEFINITIONTYPE));
        // Keep current native renderer settings
        pVideoPort->oPortDef.format.video.pNativeRender = pNativeRender;
        pNvxVideoRender->bSizeSet = OMX_TRUE;
    }


    // For now, make sure we have a default destination surface
    if (!pNvxVideoRender->pDestSurface && pVideoPort->pCurrentBufferHdr &&
        !(pVideoPort->pCurrentBufferHdr->nFlags & OMX_BUFFERFLAG_EOS))
    {
        if (pVideoPort->oPortDef.format.video.pNativeRender)
        {
            pNvxVideoRender->bUsingWindow = OMX_TRUE;

            // For now, if window mode is specified, use an overlay to
            // render to the window
            pNvxVideoRender->oRenderType = Rend_TypeOverlay;
        }

        if (pNvxVideoRender->oRenderType == Rend_TypeOverlay ||
            pNvxVideoRender->oRenderType == Rend_TypeHDMI ||
            pNvxVideoRender->oRenderType == Rend_TypeLVDS ||
            pNvxVideoRender->oRenderType == Rend_TypeCRT ||
            pNvxVideoRender->oRenderType == Rend_TypeTVout ||
            pNvxVideoRender->oRenderType == Rend_TypeSecondary)
        {
            NvError nverr = NvSuccess;
            NvU32 nWidth = pVideoPort->oPortDef.format.video.nFrameWidth;
            NvU32 nHeight = pVideoPort->oPortDef.format.video.nFrameHeight;
            NvRect rtWin = {0,0,320,240};
            NvBool bWindowDim = NV_FALSE;
            ENvxBlendType eBlendType = NVX_BLEND_NONE;
            NvU32 nLayout = NvRmSurfaceLayout_Pitch;
            NvBool bNvidiaTunneled = (pVideoPort->bNvidiaTunneling &&
                (pVideoPort->eNvidiaTunnelTransaction == ENVX_TRANSACTION_GFSURFACES))
                || (pVideoPort->pCurrentBufferHdr->nFlags & OMX_BUFFERFLAG_NV_BUFFER);

            if (pNvxVideoRender->bUsingWindow)
            {
                void *hWnd = pVideoPort->oPortDef.format.video.pNativeRender;
                if ( NvxWindowGetDisplayRect( hWnd, &rtWin) )
                {
                    bWindowDim = NV_TRUE;
                }
            }
            else if (pNvxVideoRender->bOutputSizeSet)
            {
                bWindowDim = NV_TRUE;
                pNvxVideoRender->bSizeSetDuringAlloc = OMX_TRUE;
                rtWin.left = pNvxVideoRender->oPosition.nX;
                rtWin.top = pNvxVideoRender->oPosition.nY;
                rtWin.right = rtWin.left + pNvxVideoRender->oOutputSize.nWidth;
                rtWin.bottom = rtWin.top + pNvxVideoRender->oOutputSize.nHeight;
            }

            // If HDMI or TVout or LVDS or CRT or Secondary is set, don't use system window. Output to HDMI or TVout or LVDS or CRT or Secondary display
            if (pNvxVideoRender->bAllowSecondaryWindow == OMX_FALSE &&
                (pNvxVideoRender->oRenderType == Rend_TypeHDMI ||
                pNvxVideoRender->oRenderType == Rend_TypeLVDS ||
                pNvxVideoRender->oRenderType == Rend_TypeCRT ||
                pNvxVideoRender->oRenderType == Rend_TypeTVout ||
                pNvxVideoRender->oRenderType == Rend_TypeSecondary))
            {
                bWindowDim = NV_FALSE;
            }

            // Set blend type
            switch(pNvxVideoRender->eColorBlend)
            {
                case OMX_ColorBlendAlphaConstant :
                    eBlendType = NVX_BLEND_CONSTANT_ALPHA;
                    break;
                case OMX_ColorBlendAlphaPerPixel:
                    eBlendType = NVX_BLEND_PER_PIXEL_ALPHA;
                    break;
                default:
                    eBlendType = NVX_BLEND_NONE;
                    break;
            }

            // check layout :

            if (bNvidiaTunneled)
            {
                NvMMBuffer * pInBuf = (NvMMBuffer *)pVideoPort->pCurrentBufferHdr->pBuffer;
                if (pInBuf)
                {
                    nLayout = ((NvxSurface *)&pInBuf->Payload.Surfaces)->Surfaces[0].Layout;
                }
            }
            else if (pNvxVideoRender->bSourceFormatTiled)
            {
                nLayout = NvRmSurfaceLayout_Tiled;
            }

            nverr = NvxAllocateOverlay(
                &nWidth,
                &nHeight,
                pNvxVideoRender->eRequestedColorFormat,
                &pNvxVideoRender->oOverlay,
                pNvxVideoRender->oRenderType,
                bWindowDim,
                &rtWin,
                pNvxVideoRender->bColorKeySet,
                (NvU32)(pNvxVideoRender->oColorKey.nARGBColor & pNvxVideoRender->oColorKey.nARGBMask),
                eBlendType,
                (NvU32)(pNvxVideoRender->nUniformAlpha & 0xFF),
                (NvU32)pNvxVideoRender->nRotation,
                (NvU32)pNvxVideoRender->nSetRotation,
                pNvxVideoRender->bKeepAspect,
                pNvxVideoRender->overlayIndex,
                pNvxVideoRender->overlayDepth,
                pNvxVideoRender->bNoAVSync,
                pNvxVideoRender->bTurnOffWinA,
                nLayout,
                pNvxVideoRender->bForce2DOverride,
                pNvxVideoRender->pPassedInANW,
                pNvxVideoRender->pPassedInAndroidCameraPreview);
            if (nverr != NvSuccess)
            {
                OMX_ERRORTYPE eReturnVal = OMX_ErrorInsufficientResources;
                NvxSendEvent(hComponent, OMX_EventError, eReturnVal, 0, NULL);
                NV_DEBUG_PRINTF(("Error: Unable to acquire overlay surface and window\n"));
                return eReturnVal;
            }
            pNvxVideoRender->pDestSurface = pNvxVideoRender->oOverlay.pSurface;
            pNvxVideoRender->bUseFlip = OMX_TRUE;
            pNvxVideoRender->oOverlay.StereoOverlayModeFlag =
            pNvxVideoRender->StereoRendModeFlag;
        }
        else
        {
            pNvxVideoRender->pDestSurface = NvxGetPrimarySurface();
        }
        pNvxVideoRender->bNeedUpdate = OMX_TRUE;
        if (pNvxVideoRender->bSmartDimmerEnable)
        {
            NvxSmartDimmerEnable(&pNvxVideoRender->oOverlay, NV_TRUE);
        }
        if(pNvxVideoRender->StereoRendModeFlag)
        {
            eError = NvxStereoRenderingEnable(&pNvxVideoRender->oOverlay);
            if (eError != NvSuccess)
            {
                NvxSendEvent(hComponent, OMX_EventError, eError, 0, NULL);
                NV_DEBUG_PRINTF(("Error: Unable to set stereoRendering mode \n"));
                return eError;
            }
        }

    }

    if (pNvxVideoRender->bPaused)
    {
        if (pNvxVideoRender->bNeedUpdate)
        {
            NvxRendererUpdatePosition(pNvxVideoRender, pVideoPort, hComponent);
            pNvxVideoRender->bNeedUpdate = OMX_FALSE;
        }
        return OMX_ErrorNone;
    }

    if(pNvxVideoRender->bExternalAVSync && pNvxVideoRender->pAVSyncCallBack && pVideoPort->pCurrentBufferHdr) {
        OMX_BOOL bDrop = OMX_FALSE;

        pNvxVideoRender->videoMetrics.attemptedFrames++;

        // callback to external A/V sync component
        pNvxVideoRender->pAVSyncCallBack(
            pNvxVideoRender->pAVSyncAppData,
            pVideoPort->pCurrentBufferHdr->nTimeStamp,
            pVideoPort->pCurrentBufferHdr->nFlags,
            pNvxVideoRender->xScale,
            &bDrop);

        if (bDrop == OMX_TRUE) {
            NvxPortReleaseBuffer(pVideoPort, pVideoPort->pCurrentBufferHdr);
            pVideoPort->pCurrentBufferHdr = NULL;
            bDisplay = OMX_FALSE;
            pNvxVideoRender->videoMetrics.droppedFrames++;
        }
    }
    else if (!pNvxVideoRender->bNoAVSync && pVideoPort->pCurrentBufferHdr &&
        !(pVideoPort->pCurrentBufferHdr->nFlags & OMX_BUFFERFLAG_EOS) &&
        !pNvxVideoRender->bCameraPreviewMode)
    {
        if (pClockPort && pClockPort->hTunnelComponent &&
            pClockPort->oPortDef.bEnabled && pClockPort->bNvidiaTunneling)
        {
            OMX_BOOL bDrop = OMX_FALSE;
            OMX_BOOL bVidBehind = OMX_FALSE;
            OMX_BOOL bFrameTooSoon = OMX_FALSE;
            OMX_TICKS ts = pVideoPort->pCurrentBufferHdr->nTimeStamp +
                           pNvxVideoRender->nAVSyncOffset * 1000;
            OMX_U64 now, sincelast;

            if (ts < 0)
                ts = 0;

            pNvxVideoRender->videoMetrics.attemptedFrames++;
            NvxCCWaitUntilTimestamp(pClockPort->hTunnelComponent, ts, &bDrop,
                                    &bVidBehind);

            if (bDrop == OMX_TRUE)
            {
                NvMMPayloadMetadata *pPayloadMetadata = NULL;
                NvMMBuffer *pInBuf =
                    (NvMMBuffer *)pVideoPort->pCurrentBufferHdr->pBuffer;
                if (pInBuf)
                {
                    pPayloadMetadata = &pInBuf->PayloadInfo;
                    if (pPayloadMetadata->BufferMetaDataType ==
                        NvMMBufferMetadataType_DigitalZoom)
                    {
                        bDrop = (pPayloadMetadata->BufferMetaData.
                                 DigitalZoomBufferMetadata.KeepFrame == NV_FALSE);
                    }
                }
            }
            if (bDrop == OMX_TRUE)
            {
                pNvxVideoRender->videoMetrics.lateFrames++;
            }
            if (bVidBehind == OMX_TRUE)
                pNvxVideoRender->videoMetrics.delayedFrames++;

            now = NvOsGetTimeUS();
            sincelast = (now - pNvxVideoRender->nLastDisplayTime);

            // if we're behind, start counting - 3 slightly behind frames in a
            // row means starvation.  1 dropped frame counts (too late, but..)
            //
            // 8 good frames (1/4 second) in a row says to cancel the
            // starvation request
            if (bVidBehind)
            {
                if (pNvxVideoRender->nFramesSinceStarved > 0)
                    pNvxVideoRender->nFramesSinceStarved = 0;

                pNvxVideoRender->nFramesSinceStarved--;
                if (pNvxVideoRender->nFramesSinceStarved <= -3)
                {
                    pNvxVideoRender->videoMetrics.starvationRequest++;
                    NvxVideoRenderSetStarvation(pNvxVideoRender, OMX_TRUE);
                }
            }
            else
            {
                if (pNvxVideoRender->nFramesSinceStarved < 0)
                    pNvxVideoRender->nFramesSinceStarved = 0;

                pNvxVideoRender->nFramesSinceStarved++;
                if (!pNvxVideoRender->nonStarvationPosted &&
                    pNvxVideoRender->nFramesSinceStarved >= 8)
                {
                    NvxVideoRenderSetStarvation(pNvxVideoRender, OMX_FALSE);
                }
            }

            if (pNvxVideoRender->nFramesSinceStarved < -50 &&
                pNvxVideoRender->xScale > 0x10000)
            {
                pVideoPort->pCurrentBufferHdr->nFlags |= NVX_BUFFERFLAG_DECODERSLOW;
                pNvxVideoRender->bSetLate = OMX_TRUE;
            }
            else if (pNvxVideoRender->bSetLate)
            {
                pVideoPort->pCurrentBufferHdr->nFlags |= NVX_BUFFERFLAG_DECODERNOTSLOW;
                pNvxVideoRender->bSetLate = OMX_FALSE;
            }

            if ((bDrop || (bVidBehind && !pNvxVideoRender->bUseFlip)) &&
                pNvxVideoRender->nFramesSinceLastDrop > pNvxVideoRender->nFrameDropFreq)
            {
                NvxPortReleaseBuffer(pVideoPort, pVideoPort->pCurrentBufferHdr);
                pVideoPort->pCurrentBufferHdr = NULL;
                bDisplay = OMX_FALSE;
                pNvxVideoRender->nFramesSinceLastDrop = 0;
                if (ts > OMX_TICKS_PER_SECOND)
                {
                    pNvxVideoRender->videoMetrics.droppedFrames++;
                    pNvxVideoRender->nTotFrameDrops++;
                }
            }

            if (ts == 0)
                pNvxVideoRender->nLastTimeStamp = 0;

            bFrameTooSoon = OMX_FALSE;

            // drop frames here rather than the renderer if flipping, otherwise
            // there'll be corruption
            if (pNvxVideoRender->bUseFlip && pVideoPort->pCurrentBufferHdr &&
                bFrameTooSoon && sincelast < 33000)
            {
                NvxPortReleaseBuffer(pVideoPort, pVideoPort->pCurrentBufferHdr);
                pVideoPort->pCurrentBufferHdr = NULL;
                bDisplay = OMX_FALSE;
                pNvxVideoRender->videoMetrics.droppedFrames++;
            }
            else if ((bDrop || bFrameTooSoon) && sincelast >= 33000 &&
                     pNvxVideoRender->nRotation == 0 &&
                     pNvxVideoRender->nSetRotation == 0)
            {
                pNvxVideoRender->bNoAVSyncOverride = OMX_TRUE;
            }
            else
                pNvxVideoRender->bNoAVSyncOverride = OMX_FALSE;

            if (bDisplay)
                pNvxVideoRender->nLastTimeStamp = ts;
        }
    }
    else if (pNvxVideoRender->bCameraPreviewMode)
    {
       if (!pNvxVideoRender->nonStarvationPosted)
           NvxVideoRenderSetStarvation(pNvxVideoRender, NV_FALSE);

        // camera preview mode, AV sync is not necessary, just enable vsync wait always
        pNvxVideoRender->bNoAVSyncOverride = OMX_FALSE;

        // drop frames if there is pending frame in input queue
        if (NvxPortNumPendingBuffers(pVideoPort))
        {
            NvxPortReleaseBuffer(pVideoPort, pVideoPort->pCurrentBufferHdr);
            pVideoPort->pCurrentBufferHdr = NULL;
            bDisplay = OMX_FALSE;
            pNvxVideoRender->videoMetrics.droppedFrames++;
        }
    }

    if (bDisplay)
    {
        eError = NvxVideoRenderDisplayCurrentBuffer(hComponent, pNvxVideoRender);
        if (pNvxVideoRender->bSingleFrameModeIsEnabled)
        {
            pNvxVideoRender->bDisplayNextFrame = OMX_FALSE;
        }
        pNvxVideoRender->nFramesSinceLastDrop++;
    }

    if (!pVideoPort->pCurrentBufferHdr)
        NvxPortGetNextHdr(pVideoPort);

    if (pVideoPort->pCurrentBufferHdr && !pNvxVideoRender->bPaused)
        *pbMoreWork = OMX_TRUE;

    return eError;
}


/**
 * @brief     Video Renderer helper function for displaying a single frame
 * @remarks   Displays the current buffer available
 * @returns   OMX_ERRORTYPE value
 * */
static OMX_ERRORTYPE NvxVideoRenderDisplayCurrentBuffer(OMX_IN NvxComponent *hComponent, SNvxVideoRenderData *pNvxVideoRender)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_BUFFERHEADERTYPE *pInBuffer = NULL;
    NvxPort *pVideoPort = &(hComponent->pPorts[NVX_VIDREND_VIDEO_PORT]);

    int pos = pNvxVideoRender->nPos;
#ifdef BUILD_GOOGLETV
    OMX_BOOL bSentFirstFrame = pNvxVideoRender->bSentFirstFrameEvent;
#endif

    NVXTRACE((NVXT_CALLGRAPH, hComponent->eObjectType, "+NvxVideoRenderDisplayCurrentBuffer\n"));

    pInBuffer = pVideoPort->pCurrentBufferHdr;
    if (!pInBuffer)
    {
        return OMX_ErrorNone;
    }

    // Check if we're done
    if (pInBuffer->nFilledLen < 4 || ( pInBuffer->nFlags & OMX_BUFFERFLAG_EOS))
    {
        if (pNvxVideoRender->pLastFrame)
        {
            NvxVideoRenderCopyFrame(pNvxVideoRender);
            NvxVideoRenderFlush2D(pNvxVideoRender);
        }

        NvxRenderSurfaceToOverlay(&pNvxVideoRender->oOverlay,
                                  pNvxVideoRender->oOverlay.pSurface,
                                  OMX_TRUE);

        if (pNvxVideoRender->pLastFrame)
        {
            NvxPortReleaseBuffer(pVideoPort, pNvxVideoRender->pLastFrame);
            pNvxVideoRender->pLastFrame = NULL;
        }

        eError = NvxPortReleaseBuffer(pVideoPort, pInBuffer);
        return OMX_ErrorNone;
    }

    pNvxVideoRender->videoMetrics.renderedFrames++;
    pNvxVideoRender->nFramesSinceFlush++;

    if (pNvxVideoRender->bProfile || pNvxVideoRender->bSanity)
    {
        pNvxVideoRender->llStartTS[pos] = NvOsGetTimeUS();
    }

    eError = NvxVideoRenderPrepareSourceSurface(hComponent, pNvxVideoRender);
    if(NvxIsError(eError))
    {
        return eError;
    }

    if (!pNvxVideoRender->pSrcSurface || !pNvxVideoRender->pDestSurface)
    {
        return OMX_ErrorBadParameter;
    }

    if (!pNvxVideoRender->bUseFlip && pNvxVideoRender->pLastFrame)
    {
        NvxVideoRenderFlush2D(pNvxVideoRender);
        NvxPortReleaseBuffer(pVideoPort, pNvxVideoRender->pLastFrame);
        pNvxVideoRender->pLastFrame = NULL;
    }

    if (pNvxVideoRender->bNeedUpdate)
    {
        NvxRendererUpdatePosition(pNvxVideoRender, pVideoPort, hComponent);
        pNvxVideoRender->bNeedUpdate = OMX_FALSE;
    }

    if (pNvxVideoRender->bProfile || pNvxVideoRender->bSanity)
    {
        pNvxVideoRender->llPostCSC[pos] = NvOsGetTimeUS();
    }

    if (pNvxVideoRender->bUseFlip)
    {
        OMX_BOOL bWait = !(pNvxVideoRender->bNoAVSync ||
                           pNvxVideoRender->bNoAVSyncOverride);

        NvxUpdateOverlaySurface(pNvxVideoRender, OMX_TRUE, bWait);
    }
    else
        eError = NvxVideoRenderDrawToSurface(pNvxVideoRender);

    pNvxVideoRender->nLastDisplayTime = NvOsGetTimeUS();

    if (pNvxVideoRender->bProfile)
        NvxJitterToolAddPoint(pNvxVideoRender->pDeliveryJitter);

    if (pNvxVideoRender->bNeedCapture)
    {
        NvxVideoCaptureRawFrame(pNvxVideoRender, pVideoPort);
        NvOsSemaphoreSignal(pNvxVideoRender->semCapture);
        pNvxVideoRender->bNeedCapture = NV_FALSE;
    }

    if (pNvxVideoRender->bUseFlip && pNvxVideoRender->pLastFrame)
    {
        NvxPortReleaseBuffer(pVideoPort, pNvxVideoRender->pLastFrame);
        pNvxVideoRender->pLastFrame = NULL;
    }

    if (NvxIsError(eError))
    {
        return eError;
    }

    if (pNvxVideoRender->bProfile || pNvxVideoRender->bSanity)
    {
        pNvxVideoRender->llPostBlt[pos] = NvOsGetTimeUS();
        if ((pNvxVideoRender->nProfCount != 6000)&&(pos < 6000))
            pNvxVideoRender->nProfCount = (pos + 1);
        pNvxVideoRender->nPos = (pos + 1)%6000;
    }

    pNvxVideoRender->pLastFrame = pInBuffer;
    pVideoPort->pCurrentBufferHdr = NULL;

#if 0 // bUsing2D is no longer accurate
    if (pNvxVideoRender->oOverlay.bUsing2D && pNvxVideoRender->pLastFrame &&
        !pNvxVideoRender->bPaused)
    {
        NvxPortReleaseBuffer(pVideoPort, pNvxVideoRender->pLastFrame);
        pNvxVideoRender->pLastFrame = NULL;
    }
#endif

    if (!pNvxVideoRender->bSentFirstFrameEvent)
    {
        NvxSendEvent(hComponent, NVX_EventFirstFrameDisplayed,
                     pVideoPort->oPortDef.nPortIndex, 0, 0);
        pNvxVideoRender->bSentFirstFrameEvent = OMX_TRUE;
#ifdef BUILD_GOOGLETV
        pNvxVideoRender->nPreviousFPSInfoTime = NvOsGetTimeMS();
#endif
    }

#ifdef BUILD_GOOGLETV
    {
        OMX_U32 currentTime = NvOsGetTimeMS();

        if (((currentTime - pNvxVideoRender->nPreviousFPSInfoTime) >= 1000) || !bSentFirstFrame)
        {
            OMX_U32 data1 = 0;
            OMX_U32 data2 = 0;

            data1 = (pNvxVideoRender->videoMetrics.attemptedFrames -
                     pNvxVideoRender->renderFPSInfoMetrics.attemptedFrames) << 16;
            data1 = (data1 | ((pNvxVideoRender->videoMetrics.renderedFrames -
                     pNvxVideoRender->renderFPSInfoMetrics.renderedFrames) & 0xFFFF));

            data2 = (pNvxVideoRender->videoMetrics.droppedFrames -
                     pNvxVideoRender->renderFPSInfoMetrics.droppedFrames) << 16;
            data2 = (data2 | ((pNvxVideoRender->videoMetrics.lateFrames -
                     pNvxVideoRender->renderFPSInfoMetrics.lateFrames) & 0xFFFF));

            NvOsMemcpy(&(pNvxVideoRender->renderFPSInfoMetrics),
                       &(pNvxVideoRender->videoMetrics), sizeof(NVX_VIDREND_METRIC));
            pNvxVideoRender->nPreviousFPSInfoTime = currentTime;
            NvxSendEvent(hComponent, OMX_EventRendererFpsInfo, data1, data2, NULL);
        }
    }
#endif

    return eError;
}

/**
 * @brief     Video renderer's function for acquiring resources
 * @remarks   Resource allocation
 * @returns   OMX_ERRORTYPE value
 * */
static OMX_ERRORTYPE NvxVideoRenderAcquireResources(OMX_IN NvxComponent *hComponent)
{
    SNvxVideoRenderData *pNvxVideoRender = 0;
    OMX_ERRORTYPE       eError           = OMX_ErrorNone;

    NV_ASSERT(hComponent);
    NVXTRACE((NVXT_CALLGRAPH, hComponent->eObjectType, "+NvxVideoRenderAcquireResources\n"));

    eError = NvxComponentBaseAcquireResources(hComponent);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,hComponent->eObjectType, "ERROR:NvxVideoRenderAcquireResources:"
                  ":eError = %d:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    /* Acquire resources */
    pNvxVideoRender = (SNvxVideoRenderData *)hComponent->pComponentData;
    eError = NvxVideoRenderOpen( pNvxVideoRender );

    if ((hComponent->pPorts[NVX_VIDREND_VIDEO_PORT].bNvidiaTunneling) &&
        (hComponent->pPorts[NVX_VIDREND_VIDEO_PORT].eNvidiaTunnelTransaction == ENVX_TRANSACTION_GFSURFACES))
    {
        //pNvxVideoRender->pAcquireGfSurfaceFunc = NvxVideoRenderUseGfSurface;
        // TO DO: Set function that handles client input vs nv input
    }

    if ( (hComponent->pPorts[NVX_VIDREND_VIDEO_PORT].bNvidiaTunneling) &&
         (hComponent->pPorts[NVX_VIDREND_VIDEO_PORT].eNvidiaTunnelTransaction == ENVX_TRANSACTION_NONE))
    {
        //pNvxVideoRender->bSkipBlt = OMX_TRUE;
        pNvxVideoRender->bNeedUpdate = OMX_TRUE;
    }

#ifdef BUILD_GOOGLETV
    if (NvxGetSchedulerClockPort(hComponent) && pNvxVideoRender->pSchedulerClockPort)
    {
        NvxCCSetWaitForVideo(pNvxVideoRender->pSchedulerClockPort->hTunnelComponent);
    }
    else
#endif
    if (hComponent->pPorts[NVX_VIDREND_CLOCK_PORT].hTunnelComponent)
    {
        NvxCCSetWaitForVideo(hComponent->pPorts[NVX_VIDREND_CLOCK_PORT].hTunnelComponent);
    }

    return eError;
}

/**
 * @brief     Function for free-ing resources
 * @remarks   Called when component transitions back to idle and needs to release resources
 * @returns   OMX_ERRORTYPE value
 * */
static OMX_ERRORTYPE NvxVideoRenderReleaseResources(OMX_IN NvxComponent *hComponent)
{
    SNvxVideoRenderData *pNvxVideoRender = 0;
    OMX_ERRORTYPE       eError           = OMX_ErrorNone;

    NV_ASSERT(hComponent);

    NVXTRACE((NVXT_CALLGRAPH, hComponent->eObjectType, "+NvxVideoRenderReleaseResources\n"));

    /* Release resources */
    pNvxVideoRender = (SNvxVideoRenderData *)hComponent->pComponentData;
    eError = NvxVideoRenderClose( pNvxVideoRender );
    NvxCheckError(eError, NvxComponentBaseReleaseResources(hComponent));

    return eError;
}

static OMX_ERRORTYPE NvxVideoRenderCopyFrame(SNvxVideoRenderData * pNvxVideoRender)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvError Err = NvSuccess;
    const NvRmSurface *ppDstSurfaceList;
    const NvRmSurface *ppSrcSurfaceList;
    NvDdk2dSurface *pSrcDdk2dSurface = NULL;
    NvRect SrcRect, DstRect;
    NvDdk2dFixedRect SrcRectLocal;
    NvDdk2dSurfaceType eDdkSurfaceType = NvDdk2dSurfaceType_Single;

    NV_ASSERT(pNvxVideoRender);

    /* update it again because pDestSurface might become invalid in NvxUpdateOverlay */
    if (pNvxVideoRender->oRenderType == Rend_TypeOverlay ||
            pNvxVideoRender->oRenderType == Rend_TypeHDMI ||
            pNvxVideoRender->oRenderType == Rend_TypeTVout ||
            pNvxVideoRender->oRenderType == Rend_TypeSecondary)
    {
        pNvxVideoRender->pDestSurface = pNvxVideoRender->oOverlay.pSurface;
    }
    else {
        pNvxVideoRender->pDestSurface = NvxGetPrimarySurface();
    }


    if (pNvxVideoRender->pSrcSurface && pNvxVideoRender->oOverlay.pSurface)
    {
        ppDstSurfaceList = &(pNvxVideoRender->oOverlay.pSurface->Surfaces[0]);

        ppSrcSurfaceList = &(pNvxVideoRender->pSrcSurface->Surfaces[0]);

        if (pNvxVideoRender->pSrcSurface->Surfaces[0].ColorFormat == NvColorFormat_Y8)
        {
          if (pNvxVideoRender->pSrcSurface->Surfaces[1].ColorFormat == NvColorFormat_U8_V8)
            eDdkSurfaceType = NvDdk2dSurfaceType_Y_UV;
          else
            eDdkSurfaceType = NvDdk2dSurfaceType_Y_U_V;
        }
        else
        {
            eDdkSurfaceType = NvDdk2dSurfaceType_Single;
        }

        // Setup surfaces
        Err = NvDdk2dSurfaceCreate(pNvxVideoRender->h2dHandle,
                                   eDdkSurfaceType,
                                   &(pNvxVideoRender->pSrcSurface->Surfaces[0]),
                                   &pSrcDdk2dSurface);
        if (NvSuccess != Err)
        {
            eError = OMX_ErrorUndefined;
            goto L_cleanup;
        }

        if (pNvxVideoRender->pDstDdk2dSurface)
        {
            NvDdk2dSurfaceDestroy(pNvxVideoRender->pDstDdk2dSurface);
            pNvxVideoRender->pDstDdk2dSurface = NULL;
        }

        if (pNvxVideoRender->oOverlay.pSurface->Surfaces[0].ColorFormat == NvColorFormat_Y8)
        {
            eDdkSurfaceType = NvDdk2dSurfaceType_Y_U_V;
        }
        else
        {
            eDdkSurfaceType = NvDdk2dSurfaceType_Single;
        }

        Err = NvDdk2dSurfaceCreate(pNvxVideoRender->h2dHandle,
                                   eDdkSurfaceType,
                                   &(pNvxVideoRender->oOverlay.pSurface->Surfaces[0]),
                                   &pNvxVideoRender->pDstDdk2dSurface);
        if (NvSuccess != Err)
        {
            eError = OMX_ErrorUndefined;
            goto L_cleanup;
        }

        // Set attributes
        NvDdk2dSetBlitFilter(&pNvxVideoRender->TexParam, NvDdk2dStretchFilter_Nicest);
        NvDdk2dSetBlitTransform(&pNvxVideoRender->TexParam, NvDdk2dTransform_None);

        // Set the cropping area exactly the same with rendering
        SrcRect.left    = pNvxVideoRender->oOverlay.srcX;
        SrcRect.top     = pNvxVideoRender->oOverlay.srcY;
        SrcRect.right   = SrcRect.left + pNvxVideoRender->oOverlay.srcW;
        SrcRect.bottom  = SrcRect.top  + pNvxVideoRender->oOverlay.srcH;

        DstRect.left   = 0;
        DstRect.top    = 0;
        DstRect.right  = pNvxVideoRender->oOverlay.srcW;
        DstRect.bottom = pNvxVideoRender->oOverlay.srcH;

        ConvertRect2Fx(&SrcRect, &SrcRectLocal);

        Err = NvDdk2dBlit(pNvxVideoRender->h2dHandle,
                          pNvxVideoRender->pDstDdk2dSurface, &DstRect,
                          pSrcDdk2dSurface, &SrcRectLocal, &pNvxVideoRender->TexParam);
        if (NvSuccess != Err)
        {
            eError = OMX_ErrorUndefined;
            goto L_cleanup;
        }

        eError = NvxVideoRenderFlush2D(pNvxVideoRender);
        NV_ASSERT(OMX_ErrorNone == eError);

        /* reset overlay offset */
        pNvxVideoRender->oOverlay.srcX = 0;
        pNvxVideoRender->oOverlay.srcY = 0;
        pNvxVideoRender->bNeedUpdate = OMX_TRUE;
    }

L_cleanup:
    if (pSrcDdk2dSurface)
        NvDdk2dSurfaceDestroy(pSrcDdk2dSurface);

    return eError;
}

/**
 * @brief     Flush state
 * @remarks   Flush state
 * @returns   OMX_ERRORTYPE value
 * */
static OMX_ERRORTYPE NvxVideoRenderFlush(OMX_IN NvxComponent *hComponent, OMX_U32 nPort)
{
    SNvxVideoRenderData * pNvxVideoRender = hComponent->pComponentData;

    NVXTRACE((NVXT_CALLGRAPH, hComponent->eObjectType, "+NvxVideoRenderFlush\n"));

    if (pNvxVideoRender->bUseFlip && pNvxVideoRender->pLastFrame)
    {
        // Instead of displaying a black screen, display the last frame
        NvxVideoRenderCopyFrame(pNvxVideoRender);
        NvxRenderSurfaceToOverlay(&pNvxVideoRender->oOverlay,
                                  pNvxVideoRender->oOverlay.pSurface,
                                  OMX_TRUE);
    }

    if (pNvxVideoRender->pLastFrame)
    {
        NvxPortReleaseBuffer(&hComponent->pPorts[NVX_VIDREND_VIDEO_PORT],
                             pNvxVideoRender->pLastFrame);
        pNvxVideoRender->pLastFrame = NULL;

    }

    pNvxVideoRender->bSentFirstFrameEvent = OMX_FALSE;
    pNvxVideoRender->nFramesSinceFlush = 0;

#ifdef BUILD_GOOGLETV
    NvOsMemset(&(pNvxVideoRender->renderFPSInfoMetrics), 0, (sizeof(NVX_VIDREND_METRIC)));
    pNvxVideoRender->nPreviousFPSInfoTime = 0;
#endif

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxVideoRenderPreCheckChangeState(OMX_IN NvxComponent *pNvComp,OMX_IN OMX_STATETYPE oNewState)
{
    if (oNewState == OMX_StatePause && pNvComp->eState == OMX_StateExecuting)
    {
        NvxPort *pClockPort = &pNvComp->pPorts[NVX_VIDREND_CLOCK_PORT];
#ifdef BUILD_GOOGLETV
        SNvxVideoRenderData *pNvxVideoRender = (SNvxVideoRenderData *)pNvComp->pComponentData;

        if (NvxGetSchedulerClockPort(pNvComp) && pNvxVideoRender->pSchedulerClockPort)
        {
            pClockPort = pNvxVideoRender->pSchedulerClockPort;
        }
#endif

        if (pClockPort && pClockPort->hTunnelComponent)
        {
            NvxCCUnblockWaitUntilTimestamp(pClockPort->hTunnelComponent);
        }
    }

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxVideoRenderPreChangeState(OMX_IN NvxComponent *pNvComp,OMX_IN OMX_STATETYPE oNewState)
{
    SNvxVideoRenderData * pNvxVideoRender = pNvComp->pComponentData;

    switch (oNewState)
    {
        case OMX_StateIdle:
            if (pNvComp->eState == OMX_StateExecuting)
            {
                if (pNvxVideoRender->pLastFrame)
                {
                    NvxVideoRenderCopyFrame(pNvxVideoRender);
                    NvxVideoRenderFlush2D(pNvxVideoRender);
                }

                NvxRenderSurfaceToOverlay(&pNvxVideoRender->oOverlay,
                                          pNvxVideoRender->oOverlay.pSurface,
                                          OMX_TRUE);
                NvxVideoRenderFlush2D(pNvxVideoRender);
                NvxVideoRenderSetStarvation(pNvxVideoRender, OMX_FALSE);
            }

            pNvxVideoRender->bSentFirstFrameEvent = OMX_FALSE;
            pNvxVideoRender->nFramesSinceFlush = 0;

            if (pNvxVideoRender->pLastFrame)
            {
                NvxPortReleaseBuffer(&pNvComp->pPorts[NVX_VIDREND_VIDEO_PORT],
                                     pNvxVideoRender->pLastFrame);
                pNvxVideoRender->pLastFrame = NULL;

            }

#ifdef BUILD_GOOGLETV
            pNvxVideoRender->bCheckedSchedulerClockPort = OMX_FALSE;
            pNvxVideoRender->pSchedulerClockPort = NULL;
#endif

            break;

#ifdef BUILD_GOOGLETV
        case OMX_StatePause:
            NvOsMemset(&(pNvxVideoRender->renderFPSInfoMetrics), 0, (sizeof(NVX_VIDREND_METRIC)));
            pNvxVideoRender->nPreviousFPSInfoTime = 0;
            break;
#endif
        default: break;
    }

    return OMX_ErrorNone;
}

/**
 * @brief     Hook so that the component is notified on state transitions
 * @remarks   Handles state transition specifics
 * @returns   OMX_ERRORTYPE value
 * */
static OMX_ERRORTYPE NvxVideoRenderChangeState(OMX_IN NvxComponent *pNvComp,OMX_IN OMX_STATETYPE oNewState)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    SNvxVideoRenderData * pNvxVideoRender = pNvComp->pComponentData;


    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVideoRenderChangeState: %s\n",pNvComp->pComponentName));

    NVXTRACE((NVXT_STATE, pNvComp->eObjectType, "State change started: %s from %ld to %ld\n",
              pNvComp->pComponentName, pNvComp->eState, oNewState));

    pNvxVideoRender->bPaused = OMX_FALSE;

    /* change state */
    switch (oNewState)
    {
        case OMX_StateInvalid: break;
        case OMX_StateLoaded: break;
        case OMX_StateIdle:
        {
            //NvxVideoRenderOverlayStop(pNvxVideoRender);
            //pNvxVideoRender->bOverlaySet = OMX_FALSE;
        }
        break;
        case OMX_StatePause:
        {
            NvxPort* pVideoPort = &pNvComp->pPorts[NVX_VIDREND_VIDEO_PORT];
            int i = 0;
            pNvxVideoRender->bPaused = OMX_TRUE;
            while (!pVideoPort->pCurrentBufferHdr && i < 130)
            {
                NvxPortGetNextHdr(pVideoPort);
                i++;
                if (!pVideoPort->pCurrentBufferHdr)
                    NvOsSleepMS(1);
            }
            pNvxVideoRender->videoMetrics.attemptedFrames++;
            NvxVideoRenderSetStarvation(pNvxVideoRender, OMX_FALSE);
            NvxVideoRenderDisplayCurrentBuffer(pNvComp, pNvxVideoRender);
            break;
        }
        case OMX_StateWaitForResources:
        {
            //pNvxVideoRender->bOverlaySet = OMX_FALSE;
        }
        break;
        case OMX_StateExecuting:
        {
            //pNvxVideoRender->bOverlaySet = OMX_FALSE;
            pNvxVideoRender->bNeedUpdate = OMX_TRUE;
            NvxVideoRenderSetStarvation(pNvxVideoRender, OMX_TRUE);
            NvxVideoRenderSetBusy(pNvxVideoRender, 500);
        }
        break;
        default:
            break;
    }

    return eError;
}

/* =========================================================================
 *                     I N T E R N A L
 * ========================================================================= */

static OMX_ERRORTYPE NvxVideoRenderSetup(SNvxVideoRenderData * pNvxVideoRender)
{
    NVXTRACE((NVXT_CALLGRAPH,NVXT_VIDEO_RENDERER, "+NvxVideoRenderSetup\n"));

    pNvxVideoRender->pSrcSurface  = NULL;
    pNvxVideoRender->pDestSurface = NULL;
    pNvxVideoRender->eMirror      = OMX_MirrorNone;
    pNvxVideoRender->eColorBlend  = OMX_ColorBlendNone;
    pNvxVideoRender->bInternalSurfAlloced = NV_FALSE;
    pNvxVideoRender->eRequestedColorFormat = NvColorFormat_A8R8G8B8;
    pNvxVideoRender->bUseFlip = NV_FALSE;
    pNvxVideoRender->nFrameDropFreq = 5;
    pNvxVideoRender->bKeepAspect = NV_TRUE;
    //pNvxVideoRender->eSurfaceColorFormat = pNvxVideoRender->eRequestedColorFormat;
    pNvxVideoRender->nRotation = NvxGetOsRotation();
    pNvxVideoRender->bNeedCapture = NV_FALSE;
    pNvxVideoRender->pCaptureBuffer = NULL;
    pNvxVideoRender->xScale = 0x10000;

    return OMX_ErrorNone;
}


static OMX_ERRORTYPE NvxVideoRenderOpen( SNvxVideoRenderData * pNvxVideoRender )
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    NvError nverr;

    NVXTRACE((NVXT_CALLGRAPH,NVXT_VIDEO_RENDERER, "+NvxVideoRenderOpen\n"));

    pNvxVideoRender->pDeliveryJitter = NvxAllocJitterTool("frame delivery", 100);
    NvxJitterToolSetShow(pNvxVideoRender->pDeliveryJitter, pNvxVideoRender->bShowJitter);

    pNvxVideoRender->videoMetrics.attemptedFrames = 0;
    pNvxVideoRender->videoMetrics.renderedFrames = 0;
    pNvxVideoRender->videoMetrics.delayedFrames = 0;
    pNvxVideoRender->videoMetrics.droppedFrames = 0;
    pNvxVideoRender->videoMetrics.lateFrames = 0;
    pNvxVideoRender->videoMetrics.starvationRequest = 0;
    nverr = NvRmOpen(&pNvxVideoRender->hRmDevice, 0);
    if(NvSuccess != nverr)
    {
        // Is there a macro to check what type of NvError?
        return OMX_ErrorUndefined;
    }

    nverr = NvDdk2dOpen(pNvxVideoRender->hRmDevice, NULL,
                        &(pNvxVideoRender->h2dHandle));
    if (NvSuccess != nverr)
        return OMX_ErrorUndefined;

    pNvxVideoRender->dfsClient = 0;
    pNvxVideoRender->nonStarvationPosted = 0;

    if (NvRmDfsGetState(pNvxVideoRender->hRmDevice) ==
        NvRmDfsRunState_ClosedLoop)
    {
        pNvxVideoRender->dfsClient = NVRM_POWER_CLIENT_TAG('O','M','X','*');
        nverr = NvRmPowerRegister(pNvxVideoRender->hRmDevice, NULL,
                                  &(pNvxVideoRender->dfsClient));
        if (NvSuccess != nverr)
        {
            pNvxVideoRender->dfsClient = 0;
        }
    }

    if (pNvxVideoRender->bCrcDump)
    {
        nverr = NvOsFopen("VidRender.txt",
            NVOS_OPEN_CREATE|NVOS_OPEN_WRITE,
            &pNvxVideoRender->fpCRC);
        if (nverr != NvSuccess)
            pNvxVideoRender->fpCRC = NULL;
        pNvxVideoRender->nFrameCount = 0;
    }

    pNvxVideoRender->nDBWriteFrame = 0;
    pNvxVideoRender->nDBDrawFrame  = 0;
    pNvxVideoRender->nScreenRotation = NvxGetOsRotation(); // cache os rot per instance

    nverr = NvOsSemaphoreCreate(&pNvxVideoRender->semCapture, 0);
    if(NvSuccess != nverr)
    {
        return OMX_ErrorUndefined;
    }

    return err;
}

static OMX_ERRORTYPE NvxVideoRenderClose( SNvxVideoRenderData * pNvxVideoRender )
{
    if (pNvxVideoRender->pDstDdk2dSurface)
    {
        NvDdk2dSurfaceDestroy(pNvxVideoRender->pDstDdk2dSurface);
        pNvxVideoRender->pDstDdk2dSurface = NULL;
    }

    if (pNvxVideoRender->oOverlay.pSurface)
        NvxReleaseOverlay(&pNvxVideoRender->oOverlay);

    pNvxVideoRender->pDestSurface = NULL;
    pNvxVideoRender->bSizeSet = OMX_FALSE;

    if (pNvxVideoRender->bInternalSurfAlloced)
    {
        NvxSurfaceFree(&pNvxVideoRender->pSrcSurface);
        pNvxVideoRender->bInternalSurfAlloced = NV_FALSE;
        pNvxVideoRender->nDBWriteFrame = 0;
        pNvxVideoRender->nDBDrawFrame  = 0;
    }

    if (pNvxVideoRender->bSmartDimmerEnable)
        NvxSmartDimmerEnable(&pNvxVideoRender->oOverlay, NV_FALSE);

    if (pNvxVideoRender->dfsClient)
        NvRmPowerUnRegister(pNvxVideoRender->hRmDevice,
                            pNvxVideoRender->dfsClient);
    pNvxVideoRender->dfsClient = 0;

    if (pNvxVideoRender->h2dHandle) {
        NvDdk2dClose(pNvxVideoRender->h2dHandle);
        pNvxVideoRender->h2dHandle = NULL;
    }

    // do we actually allocate rm device?
    NvRmClose(pNvxVideoRender->hRmDevice);
    pNvxVideoRender->hRmDevice = NULL;

    NvOsSemaphoreDestroy(pNvxVideoRender->semCapture);
    pNvxVideoRender->semCapture = NULL;

    if (pNvxVideoRender->bProfile)
    {
        NvOsFileHandle oOut = NULL;
        NvError status;
        NvU32 i, j = 0;
        NvU64 nInstFPS[5] = {0,0,0,0,0};
        NvU64 nInstAvgFPS[5] = {0,0,0,0,0};
        NvU64 maxTime = 0, minTime = (NvU64)-1, totTime = 0;
        double lowInst = 99999999.9;
        double lowAvg  = 99999999.9;
        double fJitterAvg = 0, fJitterStd = 0, fJitterHighest = 0;

        status = NvOsFopen(pNvxVideoRender->ProfileFileName,
                           NVOS_OPEN_CREATE|NVOS_OPEN_WRITE,
                           &oOut);

        if(status != NvSuccess)
        {
            oOut = NULL;
            NvOsDebugPrintf("NvOsFopen FAILED for %s\n", pNvxVideoRender->ProfileFileName);
        }


        //NvOsFprintf(oOut, "Frame\t\tStart Time\t\tAfter CSC\t\tEnd Time\t\tTot Time\t\tInst FPS\n");
        for (i = 0; i < pNvxVideoRender->nProfCount; i++)
        {
            double inst = 0, avg = 0;
            NvU64 fTime;

            fTime = pNvxVideoRender->llPostBlt[i] -
                    pNvxVideoRender->llStartTS[i];

            totTime += fTime;

            if (i == 0)
            {
                nInstAvgFPS[j] = 0;
            }
            else
            {
                nInstAvgFPS[j] = (pNvxVideoRender->llPostBlt[i] -
                                  pNvxVideoRender->llPostBlt[i-1]);
            }

            nInstFPS[j] = fTime;
            j = (j + 1) % 5;

            if (pNvxVideoRender->llPostBlt[i] < minTime)
                minTime = pNvxVideoRender->llPostBlt[i];
            if (pNvxVideoRender->llPostBlt[i] > maxTime)
                maxTime = pNvxVideoRender->llPostBlt[i];

            if (i > 5)
            {
                inst = (nInstFPS[0] + nInstFPS[1] + nInstFPS[2] +
                        nInstFPS[3] + nInstFPS[4]) / 5000000.0;
                inst = 1.0 / inst;
                if (inst > 0 && inst < lowInst)
                    lowInst = inst;

                avg = (nInstAvgFPS[0] + nInstAvgFPS[1] + nInstAvgFPS[2] +
                       nInstAvgFPS[3] + nInstAvgFPS[4]) / 5000000.0;
                avg = 1.0 / avg;
                if (avg > 0 && avg < lowAvg)
                    lowAvg = avg;
            }
            //if(oOut)
            //{
            //  NvOsFprintf(oOut, "%4d\t\t%f\t\t%f\t\t%f\t\t%f\t\t%f\n", i,
            //            pNvxVideoRender->llStartTS[i] / 1000000.0,
            //            pNvxVideoRender->llPostCSC[i] / 1000000.0,
            //            pNvxVideoRender->llPostBlt[i] / 1000000.0,
            //            fTime / 1000000.0, inst);
            //}
        }

        if(oOut)
        {
            NvOsFprintf(oOut, "\n");
              NvOsFprintf(oOut, "Total display time (display only): %f\n",
                          totTime / 1000000.0);
              NvOsFprintf(oOut, "Total display time (walltime): %f\n",
                        (maxTime - minTime) / 1000000.0);
            if (pNvxVideoRender->nProfCount)
            {
                NvOsFprintf(oOut, "Average FPS (display): %f\n",
                            1.0 / (totTime / pNvxVideoRender->nProfCount / 1000000.0));
                NvOsFprintf(oOut, "Average FPS (walltime): %f\n",
                            1.0 / ((maxTime - minTime) / pNvxVideoRender->nProfCount / 1000000.0));
            }
            NvOsFprintf(oOut, "Lowest instantaneous FPS: %f\n", lowInst);
            NvOsFprintf(oOut, "Lowest average real FPS: %f\n", lowAvg);
            NvOsFprintf(oOut, "\n");
            NvOsFprintf(oOut, "attempted frames = %d\n", pNvxVideoRender->videoMetrics.attemptedFrames);
            NvOsFprintf(oOut, "rendered frames = %d\n", pNvxVideoRender->videoMetrics.renderedFrames);
            NvOsFprintf(oOut, "dropped frames = %d\n", pNvxVideoRender->videoMetrics.droppedFrames);
            NvOsFprintf(oOut, "very late frames > 80ms = %d\n", pNvxVideoRender->videoMetrics.lateFrames);
            NvOsFprintf(oOut, "delayed frames > 15ms  = %d\n", pNvxVideoRender->videoMetrics.delayedFrames);
            NvOsFprintf(oOut, "starvation notifications = %d\n", pNvxVideoRender->videoMetrics.starvationRequest);
            NvxJitterToolGetAvgs(pNvxVideoRender->pDeliveryJitter,
                                 &fJitterStd, &fJitterAvg, &fJitterHighest);
            NvOsFprintf(oOut, "\n");
            NvOsFprintf(oOut, "Average jitter: %f uSec\n", fJitterStd);
            NvOsFprintf(oOut, "Highest instantaneous jitter: %f uSec\n", fJitterHighest);
            NvOsFclose(oOut);
        }

        // This information matches what AwesomePlayer spits out
        NvOsDebugPrintf("--------Video Statistics------------");
        NvOsDebugPrintf("--------Video Duration = %lld sec", (maxTime - minTime) / 1000000);
        if (pNvxVideoRender->nProfCount)
        {
            NvOsDebugPrintf("--------Avg playback fps = %.2f",
                            1.0 / ((maxTime - minTime) / pNvxVideoRender->nProfCount / 1000000.0));
        }
        NvOsDebugPrintf("--------Dropped frames = %d", pNvxVideoRender->videoMetrics.droppedFrames);
        NvOsDebugPrintf("--------Rendered frames = %d", pNvxVideoRender->videoMetrics.renderedFrames);

        NvxJitterToolGetAvgs(pNvxVideoRender->pDeliveryJitter, &fJitterStd, &fJitterAvg, &fJitterHighest);
        NvOsDebugPrintf("\n");
        NvOsDebugPrintf("--------Jitter Statistics------------");
        NvOsDebugPrintf("--------Average jitter = %f uSec \n", fJitterStd);
        NvOsDebugPrintf("--------Highest instantaneous jitter = %f uSec \n", fJitterHighest);
        NvOsDebugPrintf("--------Mean time between frame(used in jitter) = %f uSec \n", fJitterAvg);
        NvOsDebugPrintf("\n");

        // The rest of the information is extra vidrend info that AwesomePlayer doesn't display.
        NvOsDebugPrintf("Total display time (display only): %f\n", totTime / 1000000.0);
        if (pNvxVideoRender->nProfCount)
        {
            NvOsDebugPrintf("Average FPS (display): %f\n",
                            1.0 / (totTime / pNvxVideoRender->nProfCount / 1000000.0));
        }
        NvOsDebugPrintf("Lowest instantaneous FPS: %f\n", lowInst);
        NvOsDebugPrintf("Lowest average real FPS: %f\n", lowAvg);

        NvOsDebugPrintf("\n");
        NvOsDebugPrintf("attempted frames = %d\n", pNvxVideoRender->videoMetrics.attemptedFrames);
        NvOsDebugPrintf("very late frames > 80ms = %d\n", pNvxVideoRender->videoMetrics.lateFrames);
        NvOsDebugPrintf("delayed frames > 15ms  = %d\n", pNvxVideoRender->videoMetrics.delayedFrames);
        NvOsDebugPrintf("starvation notifications = %d\n", pNvxVideoRender->videoMetrics.starvationRequest);
    }

    NvxFreeJitterTool(pNvxVideoRender->pDeliveryJitter);
    pNvxVideoRender->pDeliveryJitter = NULL;

    if (pNvxVideoRender->bCrcDump)
    {
        if (pNvxVideoRender->fpCRC)
        {
            NvOsFclose(pNvxVideoRender->fpCRC);
        }
        pNvxVideoRender->fpCRC = NULL;
    }

    return OMX_ErrorNone;
}

// Component name/role helper functions
static OMX_STRING NvxVideoRenderGetComponentName( OMX_COLOR_FORMATTYPE eColorFormat, ENvxVidRendType oType)
{
    OMX_U32 index = 0;
    NVX_VIDREND_STR_PAIR *pTable;
    OMX_U32 nLimit;

    switch (oType)
    {
        default:
        case Rend_TypePrimary:
            pTable = s_pVideoRendererNameTable;
            nLimit = sizeof(s_pVideoRendererNameTable) / sizeof(NVX_VIDREND_STR_PAIR);
            break;
        case Rend_TypeOverlay:
            pTable = s_pOverlayRendererNameTable;
            nLimit = sizeof(s_pOverlayRendererNameTable) / sizeof(NVX_VIDREND_STR_PAIR);
            break;
        case Rend_TypeHDMI:
            pTable = s_pHdmiRendererNameTable;
            nLimit = sizeof(s_pHdmiRendererNameTable) / sizeof(NVX_VIDREND_STR_PAIR);
            break;
        case Rend_TypeLVDS:
            pTable = s_pLvdsRendererNameTable;
            nLimit = sizeof(s_pLvdsRendererNameTable) / sizeof(NVX_VIDREND_STR_PAIR);
            break;
        case Rend_TypeCRT:
            pTable = s_pCrtRendererNameTable;
            nLimit = sizeof(s_pCrtRendererNameTable) / sizeof(NVX_VIDREND_STR_PAIR);
            break;
        case Rend_TypeTVout:
            pTable = s_pTvoutRendererNameTable;
            nLimit = sizeof(s_pTvoutRendererNameTable) / sizeof(NVX_VIDREND_STR_PAIR);
            break;
        case Rend_TypeSecondary:
            pTable = s_pSecondaryRendererNameTable;
            nLimit = sizeof(s_pSecondaryRendererNameTable) / sizeof(NVX_VIDREND_STR_PAIR);
            break;
    }

    for (index = 0; index < nLimit; index++)
    {
        if (pTable[index].eColorFormat == eColorFormat)
            return pTable[index].szName;
    }

    return NULL;
}


static OMX_ERRORTYPE NvxVideoRenderSetVRCompRoles( NvxComponent *pNvComp, OMX_COLOR_FORMATTYPE eColorFormat)
{
    switch( eColorFormat )
    {
        case OMX_COLOR_Format16bitRGB565:
            pNvComp->nComponentRoles = 1;
            pNvComp->sComponentRoles[0] = "iv_renderer.rgb565";
            return OMX_ErrorNone;
        case OMX_COLOR_Format32bitARGB8888:
            pNvComp->nComponentRoles = 1;
            pNvComp->sComponentRoles[0] = "iv_renderer.argb8888";
            return OMX_ErrorNone;
        default: break;
    }
    return OMX_ErrorUndefined;
}

static OMX_ERRORTYPE NvxVideoRenderSetOVCompRoles( NvxComponent *pNvComp, OMX_COLOR_FORMATTYPE eColorFormat)
{
    switch( eColorFormat )
    {
        case OMX_COLOR_Format16bitRGB565:
            pNvComp->nComponentRoles = 1;
            pNvComp->sComponentRoles[0] = "iv_renderer.rgb.overlay";
            return OMX_ErrorNone;
        case OMX_COLOR_FormatYUV420Planar:
            pNvComp->nComponentRoles = 1;
            pNvComp->sComponentRoles[0] = "iv_renderer.yuv.overlay";
            return OMX_ErrorNone;
        case OMX_COLOR_Format32bitARGB8888:
            pNvComp->nComponentRoles = 1;
            pNvComp->sComponentRoles[0] = "iv_renderer.argb8888.overlay";
            return OMX_ErrorNone;
        default: break;
    }
    return OMX_ErrorUndefined;
}

static OMX_ERRORTYPE NvxVideoRenderSetHdmiCompRoles( NvxComponent *pNvComp, OMX_COLOR_FORMATTYPE eColorFormat)
{
    switch( eColorFormat )
    {
        case OMX_COLOR_FormatYUV420Planar:
            pNvComp->nComponentRoles = 1;
            pNvComp->sComponentRoles[0] = "iv_renderer.hdmi.yuv420";
            return OMX_ErrorNone;
        default: break;
    }
    return OMX_ErrorUndefined;
}

static OMX_ERRORTYPE NvxVideoRenderSetLvdsCompRoles( NvxComponent *pNvComp, OMX_COLOR_FORMATTYPE eColorFormat)
{
    switch( eColorFormat )
    {
        case OMX_COLOR_FormatYUV420Planar:
            pNvComp->nComponentRoles = 1;
            pNvComp->sComponentRoles[0] = "iv_renderer.lvds.yuv420";
            return OMX_ErrorNone;
        default: break;
    }
    return OMX_ErrorUndefined;
}

static OMX_ERRORTYPE NvxVideoRenderSetCrtCompRoles( NvxComponent *pNvComp, OMX_COLOR_FORMATTYPE eColorFormat)
{
    switch( eColorFormat )
    {
        case OMX_COLOR_FormatYUV420Planar:
            pNvComp->nComponentRoles = 1;
            pNvComp->sComponentRoles[0] = "iv_renderer.crt.yuv420";
            return OMX_ErrorNone;
        default: break;
    }
    return OMX_ErrorUndefined;
}

static OMX_ERRORTYPE NvxVideoRenderSetTvoutCompRoles( NvxComponent *pNvComp, OMX_COLOR_FORMATTYPE eColorFormat)
{
    switch( eColorFormat )
    {
        case OMX_COLOR_FormatYUV420Planar:
            pNvComp->nComponentRoles = 1;
            pNvComp->sComponentRoles[0] = "iv_renderer.tvout.yuv420";
            return OMX_ErrorNone;
        default: break;
    }
    return OMX_ErrorUndefined;
}

static OMX_ERRORTYPE NvxVideoRenderSetSecondaryCompRoles( NvxComponent *pNvComp, OMX_COLOR_FORMATTYPE eColorFormat)
{
    switch( eColorFormat )
    {
        case OMX_COLOR_FormatYUV420Planar:
            pNvComp->nComponentRoles = 1;
            pNvComp->sComponentRoles[0] = "iv_renderer.secondary.yuv420";
            return OMX_ErrorNone;
        default: break;
    }
    return OMX_ErrorUndefined;
}

static OMX_ERRORTYPE NvxVideoRenderSetComponentRoles( NvxComponent *pNvComp, OMX_COLOR_FORMATTYPE eColorFormat)
{
    SNvxVideoRenderData *pNvxVideoRender = (SNvxVideoRenderData *)pNvComp->pComponentData;

    switch (pNvxVideoRender->oRenderType)
    {
        default:
        case Rend_TypePrimary:
            return NvxVideoRenderSetVRCompRoles(pNvComp, eColorFormat);
        case Rend_TypeOverlay:
            return NvxVideoRenderSetOVCompRoles(pNvComp, eColorFormat);
        case Rend_TypeHDMI:
            return NvxVideoRenderSetHdmiCompRoles(pNvComp, eColorFormat);
        case Rend_TypeLVDS:
            return NvxVideoRenderSetLvdsCompRoles(pNvComp, eColorFormat);
        case Rend_TypeCRT:
            return NvxVideoRenderSetCrtCompRoles(pNvComp, eColorFormat);
        case Rend_TypeTVout:
            return NvxVideoRenderSetTvoutCompRoles(pNvComp, eColorFormat);
        case Rend_TypeSecondary:
            return NvxVideoRenderSetSecondaryCompRoles(pNvComp, eColorFormat);
    }
}

static OMX_ERRORTYPE NvxVideoRenderPortEvent(NvxComponent *pNvComp,
                                              OMX_U32 nPort, OMX_U32 uEventType)
{
    SNvxVideoRenderData *pNvxVideoRender = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxPort* pVideoPort = &pNvComp->pPorts[NVX_VIDREND_VIDEO_PORT];

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVideoRenderPortEvent\n"));

    pNvxVideoRender = (SNvxVideoRenderData *)pNvComp->pComponentData;

    if (nPort == NVX_VIDREND_VIDEO_PORT)
    {
        if (uEventType)
        {
            pNvxVideoRender->bSizeSet = OMX_FALSE;
#ifdef BUILD_GOOGLETV
            pNvxVideoRender->bCheckedSchedulerClockPort = OMX_FALSE;
            pNvxVideoRender->pSchedulerClockPort = NULL;
#endif
        }
        else
        {
            if (pNvxVideoRender->pLastFrame)
            {
                NvxPortReleaseBuffer(pVideoPort, pNvxVideoRender->pLastFrame);
                pNvxVideoRender->pLastFrame = NULL;
            }
            pNvxVideoRender->pSrcSurface = NULL;
        }
    }

    return eError;
}

static void NvxVideoRenderRegisterFunctions( NvxComponent *pNvComp, OMX_COLOR_FORMATTYPE eColorFormat)
{
    SNvxVideoRenderData *pNvxVideoRender = (SNvxVideoRenderData *)pNvComp->pComponentData;

    switch( eColorFormat )
    {
        case OMX_COLOR_Format16bitRGB565:
            pNvxVideoRender->eRequestedColorFormat = NvColorFormat_R5G6B5;
            break;
        case OMX_COLOR_FormatYUV420Planar:
            pNvxVideoRender->eRequestedColorFormat = NvColorFormat_Y8;
            break;
        case OMX_COLOR_Format32bitARGB8888:
            pNvxVideoRender->eRequestedColorFormat = NvColorFormat_A8R8G8B8;
            break;
        default:
            pNvxVideoRender->eRequestedColorFormat = NvColorFormat_A8R8G8B8;
            break;
    }

    pNvComp->SetParameter       = NvxVideoRenderSetParameter;
    pNvComp->GetConfig          = NvxVideoRenderGetConfig;
    pNvComp->SetConfig          = NvxVideoRenderSetConfig;
    pNvComp->WorkerFunction     = NvxVideoRenderWorkerFunction;
    pNvComp->ReleaseResources   = NvxVideoRenderReleaseResources;
    pNvComp->AcquireResources   = NvxVideoRenderAcquireResources;
    pNvComp->eObjectType        = NVXT_VIDEO_RENDERER;
    pNvComp->PortEventHandler   = NvxVideoRenderPortEvent;
}

static OMX_ERRORTYPE NvxVideoRenderPrepareSourceSurface(
    OMX_IN NvxComponent *hComponent,
    SNvxVideoRenderData * pNvxVideoRender)
{
    NvxPort *pInPort = pNvxVideoRender->pInPort;
    OMX_U32 nWidth = pInPort->oPortDef.format.video.nFrameWidth;
    OMX_U32 nHeight = pInPort->oPortDef.format.video.nFrameHeight;
    NvMMBuffer *pInBuf = NULL;
    NvError err;
    NvBool bNvidiaTunneled = NV_FALSE;

    if (!pInPort->pCurrentBufferHdr)
    {
        return OMX_ErrorNone;
    }

    // check whether we're tunneled w/ NVIDIA components:
    bNvidiaTunneled =
        ((hComponent->pPorts[NVX_VIDREND_VIDEO_PORT].bNvidiaTunneling) &&
        (hComponent->pPorts[NVX_VIDREND_VIDEO_PORT].eNvidiaTunnelTransaction == ENVX_TRANSACTION_GFSURFACES))
        || (hComponent->pPorts[NVX_VIDREND_VIDEO_PORT].pCurrentBufferHdr->nFlags & OMX_BUFFERFLAG_NV_BUFFER);

    if (bNvidiaTunneled)
    {
        pInBuf = (NvMMBuffer *)pInPort->pCurrentBufferHdr->pBuffer;
        pNvxVideoRender->pSrcSurface = (NvxSurface *)&pInBuf->Payload.Surfaces;
        pNvxVideoRender->oOverlay.StereoOverlayModeFlag |= pInBuf->PayloadInfo.BufferFlags;
    }
    else
    {
        if (!pNvxVideoRender->bInternalSurfAlloced && pInPort->bAllocateRmSurf &&
            (pInPort->pCurrentBufferHdr->nFilledLen == sizeof(NvMMSurfaceDescriptor)))
        {
            NvU32 i = 0;
            for (i = 0; i < pInPort->nReqBufferCount; i++)
            {
                if (pInPort->pCurrentBufferHdr->pBuffer == pInPort->ppBufferHdrs[i]->pBuffer)
                {
                    pNvxVideoRender->pSrcSurface = (NvxSurface *)pInPort->ppRmSurf[i];
                    bNvidiaTunneled = NV_TRUE;
                    break;
                }
            }
        }
        if (!pNvxVideoRender->pSrcSurface)
        {
            // Only need a surface if the client is passing in its own OMX buffers. Otherwise, we're
            // tunneled, and can handle the color conversions in the 2D DDK

            // Default to tiled layout
            NvU32 Layout = NvRmSurfaceLayout_Pitch;

            if (pNvxVideoRender->bSourceFormatTiled)
                Layout = NvRmSurfaceLayout_Tiled;

            err = NvxSurfaceAlloc(
                &pNvxVideoRender->pSrcSurface,
                nWidth,
                nHeight,
                pNvxVideoRender->eRequestedColorFormat,
                Layout);
            if (NvSuccess != err)
            {
                NV_DEBUG_PRINTF(("Error: Unable to allocate source surface\n"));
                return OMX_ErrorInsufficientResources;
            }
            pNvxVideoRender->bInternalSurfAlloced = NV_TRUE;
        }
    }

    if (!bNvidiaTunneled)
    {
         // Not tunneled: Internal surface should exist
        NV_ASSERT(pNvxVideoRender->pSrcSurface != NULL);
        NvxWriteOMXFrameDataToNvSurf(pNvxVideoRender, pInPort->pCurrentBufferHdr);
    }

    NvxVideoRendererTestCropRect( pNvxVideoRender );

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxVideoRendererTestCropRect(SNvxVideoRenderData * pNvxVideoRender)
{
    NvxPort *pInPort = pNvxVideoRender->pInPort;

    if (pInPort && pNvxVideoRender->pSrcSurface)
    {
        OMX_U32 nWidth = pInPort->oPortDef.format.video.nFrameWidth;
        OMX_U32 nHeight = pInPort->oPortDef.format.video.nFrameHeight;
        NvRect videoCrop = pNvxVideoRender->pSrcSurface->CropRect;
        NvRect userCrop = pNvxVideoRender->rtUserCropRect;
        NvRect *actualCrop = &(pNvxVideoRender->rtActualCropRect);
        NvRect oldCrop = pNvxVideoRender->rtActualCropRect;

        actualCrop->left = 0;
        actualCrop->top = 0;
        actualCrop->right = nWidth;
        actualCrop->bottom = nHeight;

        // if there's a valid video cropping rect
        if (videoCrop.right > 0 && videoCrop.bottom > 0 &&
            videoCrop.right <= (NvS32)nWidth &&
            videoCrop.bottom <= (NvS32)nHeight &&
            videoCrop.left < videoCrop.right &&
            videoCrop.top < videoCrop.bottom)
        {
            *actualCrop = videoCrop;
        }

        // use user crop rect if it's smaller
        if (pNvxVideoRender->bUserCropRectSet &&
            (userCrop.left >= actualCrop->left &&
             userCrop.top >= actualCrop->top &&
             userCrop.right <= actualCrop->right &&
             userCrop.bottom <= actualCrop->bottom))
        {
            *actualCrop = userCrop;
        }

        // sanitize
        if (actualCrop->left < 0 || actualCrop->left > actualCrop->right)
            actualCrop->left = 0;
        if (actualCrop->top < 0 || actualCrop->top > actualCrop->bottom)
            actualCrop->bottom = 0;
        if (actualCrop->right > (NvS32)nWidth)
            actualCrop->right = nWidth;
        if (actualCrop->bottom > (NvS32)nHeight)
            actualCrop->bottom = nHeight;

        if (actualCrop->left != oldCrop.left ||
            actualCrop->top != oldCrop.top ||
            actualCrop->right != oldCrop.right ||
            actualCrop->bottom != oldCrop.bottom)
        {
            pNvxVideoRender->bCropSet = OMX_TRUE;
            pNvxVideoRender->bNeedUpdate = OMX_TRUE;
        }
    }

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxVideoRenderFlush2D(SNvxVideoRenderData *pNvxVideoRender)
{
    NvU32 NumFencesOut = 0;
    // Commit blt and wait for completion
    if (pNvxVideoRender->pDstDdk2dSurface)
    {
        NvDdk2dSurfaceLock(pNvxVideoRender->pDstDdk2dSurface,
            NvDdk2dSurfaceAccessMode_ReadWrite,
            NULL,
            pNvxVideoRender->oFences,
            &NumFencesOut);

        if (NumFencesOut > 0)
        {
            int i = 0;
            for (i = 0; i < NumFencesOut; i++)
            {
                NvRmFenceWait(pNvxVideoRender->hRmDevice,
                              &pNvxVideoRender->oFences[i], NV_WAIT_INFINITE);
            }
        }

        NvDdk2dSurfaceUnlock(pNvxVideoRender->pDstDdk2dSurface,NULL, 0);
    }

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxVideoRenderDrawToSurface(SNvxVideoRenderData * pNvxVideoRender)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvError err;
    NvDdk2dTransform transform;
    NvBool bRotate = 0;
    NvRect destRect;

    NvDdk2dSurface *pSrcDdk2dSurface = NULL;
    NvDdk2dFixedRect SrcRectLocal;

    if (!pNvxVideoRender->pDestSurface || !pNvxVideoRender->pSrcSurface)
        return OMX_ErrorNone;

    // Setup surfaces
    err = NvDdk2dSurfaceCreate(pNvxVideoRender->h2dHandle,
                               NvxMapSurfaceType(pNvxVideoRender->pSrcSurface),
                               &(pNvxVideoRender->pSrcSurface->Surfaces[0]),
                               &pSrcDdk2dSurface);
    if (NvSuccess != err)
    {
        eError = OMX_ErrorUndefined;
        goto L_cleanup;
    }

    if (pNvxVideoRender->pDstDdk2dSurface)
    {
        NvDdk2dSurfaceDestroy(pNvxVideoRender->pDstDdk2dSurface);
        pNvxVideoRender->pDstDdk2dSurface = NULL;
    }

    if (!pNvxVideoRender->bUseFlip)
    {
        err = NvDdk2dSurfaceCreate(pNvxVideoRender->h2dHandle,
                                   NvDdk2dSurfaceType_Single,
                                   &(pNvxVideoRender->pDestSurface->Surfaces[0]),
                                   &pNvxVideoRender->pDstDdk2dSurface);
    }
    else {
        err = NvDdk2dSurfaceCreate(pNvxVideoRender->h2dHandle,
                                   NvDdk2dSurfaceType_Y_U_V,
                                   &(pNvxVideoRender->pDestSurface->Surfaces[0]),
                                   &pNvxVideoRender->pDstDdk2dSurface);
    }

    if (NvSuccess != err)
    {
        eError = OMX_ErrorUndefined;
        goto L_cleanup;
    }

    /* Here is the table for how rotation and flipping interact.  This assumes
       rotation should be applied first, then flipping:

        Rotation   Normal       X-invert          Y-invert        Both-invert
        -----+----------------------------------------------------------------
        0    |     None         FlipHorizontal    FlipVertical     Rotate180
        90   |     Rotate90     InvTranspose      Transpose        Rotate270
        180  |     Rotate180    FlipVertical      FlipHorizontal   None
        270  |     Rotate270    Transpose         InvTranspose     Rotate90
    */

    switch (pNvxVideoRender->nRotation)
    {
        case 90:
        {
            switch (pNvxVideoRender->eMirror)
            {
                case OMX_MirrorHorizontal:
                    transform = NvDdk2dTransform_InvTranspose; break;
                case OMX_MirrorVertical:
                    transform = NvDdk2dTransform_Transpose; break;
                case OMX_MirrorBoth:
                    transform = NvDdk2dTransform_Rotate270; break;
                default:
                    transform = NvDdk2dTransform_Rotate90; break;
            }
            bRotate = 1;
            break;
        }
        case 180:
        {
            switch (pNvxVideoRender->eMirror)
            {
                case OMX_MirrorHorizontal:
                    transform = NvDdk2dTransform_FlipVertical; break;
                case OMX_MirrorVertical:
                    transform = NvDdk2dTransform_FlipHorizontal; break;
                case OMX_MirrorBoth:
                    transform = NvDdk2dTransform_None; break;
                default:
                    transform = NvDdk2dTransform_Rotate180; break;
            }
            break;
        }
        case 270:
        {
            switch (pNvxVideoRender->eMirror)
            {
                case OMX_MirrorHorizontal:
                    transform = NvDdk2dTransform_Transpose; break;
                case OMX_MirrorVertical:
                    transform = NvDdk2dTransform_InvTranspose; break;
                case OMX_MirrorBoth:
                    transform = NvDdk2dTransform_Rotate90; break;
                default:
                    transform = NvDdk2dTransform_Rotate270; break;
            }
            bRotate = 1;
            break;
        }
        default:  // 0
        {
            switch (pNvxVideoRender->eMirror)
            {
                case OMX_MirrorHorizontal:
                    transform = NvDdk2dTransform_FlipHorizontal; break;
                case OMX_MirrorVertical:
                    transform = NvDdk2dTransform_FlipVertical; break;
                case OMX_MirrorBoth:
                    transform = NvDdk2dTransform_Rotate180; break;
                default:
                    transform = NvDdk2dTransform_None; break;
            }
            break;
        }
    }

    // Set attributes
    NvDdk2dSetBlitFilter(&pNvxVideoRender->TexParam, NvDdk2dStretchFilter_Nicest);

    NvDdk2dSetBlitTransform(&pNvxVideoRender->TexParam, transform);

    ConvertRect2Fx(&pNvxVideoRender->rtActualCropRect, &SrcRectLocal);

    // rotate dest rectangle and scale to fit, if appropriate.
    destRect = pNvxVideoRender->rtDestRect;

    err = NvDdk2dBlit(pNvxVideoRender->h2dHandle,
                      pNvxVideoRender->pDstDdk2dSurface, &destRect,
                      pSrcDdk2dSurface, &SrcRectLocal, &pNvxVideoRender->TexParam);
    if (err != NvSuccess)
    {
        eError = OMX_ErrorUndefined;
        goto L_cleanup;
    }

    if (pNvxVideoRender->bCrcDump)
    {
        char *pOutBuf = NULL;
        NvBool bSucceed = NV_FALSE;
        NvU32 nOutSize = NvxCrcGetDigestLength();
        pOutBuf = NvOsAlloc(nOutSize);
        if(pOutBuf)
        {
            bSucceed = NvxCrcCalcNvMMSurface( pNvxVideoRender->pDestSurface,
                pOutBuf);
            if (bSucceed && pNvxVideoRender->fpCRC)
            {
                NvOsFprintf(pNvxVideoRender->fpCRC,
                    "Frame %d: %s\n", pNvxVideoRender->nFrameCount, pOutBuf);
            }
            NvOsFree(pOutBuf);
        }
        pNvxVideoRender->nFrameCount++;
    }

L_cleanup:
    if (pSrcDdk2dSurface)
        NvDdk2dSurfaceDestroy(pSrcDdk2dSurface);

    return eError;
}

/**
 * @brief     Deduce NvDdk2d surface type from surface
 * @returns   NvDdk2dSurfaceType value
 * */

static NvDdk2dSurfaceType NvxMapSurfaceType (NvMMSurfaceDescriptor* surf)
{
    if (surf->Surfaces[0].ColorFormat == NvColorFormat_Y8)
    {
        if (surf->SurfaceCount == 3)
            return NvDdk2dSurfaceType_Y_U_V;
        if (surf->SurfaceCount == 2)
            return NvDdk2dSurfaceType_Y_UV;
    }

    return NvDdk2dSurfaceType_Single;
}

#ifdef BUILD_GOOGLETV
static OMX_BOOL NvxGetSchedulerClockPort(OMX_IN NvxComponent *hComponent)
{
    SNvxVideoRenderData * pNvxVideoRender = (SNvxVideoRenderData *)hComponent->pComponentData;

    if (pNvxVideoRender->pSchedulerClockPort)
    {
        return OMX_TRUE;
    }
    else if (pNvxVideoRender->bCheckedSchedulerClockPort)
    {
        return OMX_FALSE;
    }
    else
    {
        NvxComponent * tComponent = NULL;
        NvxPort * pVideoPort = NULL;
        pVideoPort = &hComponent->pPorts[NVX_VIDREND_VIDEO_PORT];
        if (pVideoPort)
        {
            tComponent = NvxComponentFromHandle(pVideoPort->hTunnelComponent);
            //If tunneled comp is schedular then only use tunneled port
            if (tComponent && !NvOsStrcmp(tComponent->pComponentName, OMX_NV_SCHEDULER_OMX_COMP))
            {
                pNvxVideoRender->pSchedulerClockPort = &tComponent->pPorts[NVX_SCHEDULER_CLOCK_PORT];
                if (pNvxVideoRender->pSchedulerClockPort)
                {
                    return OMX_TRUE;
                }
            }
        }
    }

    pNvxVideoRender->bCheckedSchedulerClockPort = OMX_TRUE;
    return OMX_FALSE;
}
#endif
