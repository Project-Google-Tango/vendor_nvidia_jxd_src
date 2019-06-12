/* Copyright (c) 2006 - 2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __NVX_GFSDK_HELPERS_H__
#define __NVX_GFSDK_HELPERS_H__

#include <OMX_Core.h>
#include "nvcommon.h"
#include "nvrm_init.h"
#include "nvddk_disp.h"
#include "nvddk_2d_v2.h"
#include "nvmm_buffertype.h"
#include <NVOMX_IndexExtensions.h>

#ifdef USE_DC_DRIVER
#include "nvdc.h"
#endif

#define DEFAULT_TEGRA_OVERLAY_INDEX 1
#define DEFAULT_TEGRA_OVERLAY_DEPTH 0

#define NVX_MIN(a,b) ((a) > (b) ? b : a)

typedef NvMMSurfaceDescriptor NvxSurface;

OMX_ERRORTYPE NvxCreateAcqLock(void);
OMX_ERRORTYPE NvxDestroyAcqLock(void);
OMX_ERRORTYPE NvxLockAcqLock(void);
OMX_ERRORTYPE NvxUnlockAcqLock(void);

OMX_ERRORTYPE NvxInitPlatform(void);
void NvxDeinitPlatform(void);

NvDdk2dHandle NvxGet2d(void);
void NvxLock2d(void);
void NvxUnlock2d(void);
void Nvx2dFlush(NvDdk2dSurface *pDstDdk2dSurface);

NvRmDeviceHandle NvxGetRmDevice(void);
NvxSurface * NvxGetPrimarySurface(void);

NvError NvxSurfaceAllocYuv(
    NvxSurface *pSurface,
    NvU32 Width,
    NvU32 Height,
    NvU32 Format,
    NvU32 Layout,
    NvU32 *pImageSize,
    NvBool UseAlignment,
    NvOsMemAttribute  Coherency,
    NvBool UseNV21);

NvError NvxSurfaceAllocContiguousYuv(
    NvxSurface *pSurface,
    NvU32 Width,
    NvU32 Height,
    NvU32 Format, // NvMM color format. Not Ddk color format
    NvU32 Layout,
    NvU32 *pImageSize,
    NvBool UseAlignment,
    NvOsMemAttribute Coherency,
    NvBool ClrYUVsurface);

NvError NvxSurfaceAlloc(
    NvxSurface **ppSurf,
    NvU32 Width,
    NvU32 Height,
    NvColorFormat Format,
    NvU32 Layout);

NvError NvxSurfaceAllocContiguous(
    NvxSurface **ppSurf,
    NvU32 Width,
    NvU32 Height,
    NvColorFormat Format,
    NvU32 Layout);

OMX_U8* NvxAllocRmSurf(OMX_U32 *nSize,
    NvU32 Width,
    NvU32 Height,
    NvColorFormat Format,
    NvU32 Layout,
    NvxSurface **ppSurfHandle);

NvError NvxAllocateMemoryBuffer(
    NvRmDeviceHandle hRmDev,
    NvRmMemHandle *hMem,
    NvU32 size,
    NvU32 align,
    NvU32 *phyAddress,
    NvOsMemAttribute Coherency,
    NvU16 Kind);

void NvxFreeRmSurf(OMX_U8 *pData, NvU32 nSize);
void NvxSurfaceFree(NvxSurface **ppSurf);
void NvxMemoryFree(NvRmMemHandle *hMem);
// Overlay helpers

typedef enum ENvxVidRendType
{
    Rend_TypePrimary   = 0,
    Rend_TypeOverlay   = 1,
    Rend_TypeHDMI      = 2,
    Rend_TypeTVout     = 3,
    Rend_TypeNative    = 4,
    Rend_TypeSecondary = 5,
    Rend_TypeLVDS      = 6,
    Rend_TypeCRT       = 7
} ENvxVidRendType;

typedef enum ENvxBlendType
{
    NVX_BLEND_NONE = 0,
    NVX_BLEND_CONSTANT_ALPHA,
    NVX_BLEND_PER_PIXEL_ALPHA,
    NVX_BLEND_COUNT
} ENvxBlendType;

typedef struct NvxOverlay
{
    OMX_BOOL bNative;

    NvU32 StereoOverlayModeFlag;
    NvxSurface *pSurface;

    NvU32 srcX, srcY, srcW, srcH;
    NvU32 destX, destY, destW, destH;

    NvBool bKeepAspect;

    NvBool bColorKey;
    NvU32  nColorKey;
    ENvxBlendType eBlendType;
    NvU32  nBlendAlphaValue;
    ENvxVidRendType eType;

    NvU32 nRotation;
    NvU32 nSetRotation;

    NvBool bNoAVSync;
    NvBool bTurnOffWinA;

    NvBool bUsing2D;
    NvRmDeviceHandle hRmDev;

    NvBool bForce2D;

    NvU32 screenWidth, screenHeight;

    NvBool bSmartDimmerOn;
    NvUPtr pANW;
    NvUPtr pAndroidCameraPreview;

    NvU32  DisplayMask;

    NvU32  nIndex;
    NvU32  nDepth;
#ifdef USE_DC_DRIVER
    //DC driver related Variables
    OMX_BOOL bUsingDC;
    nvdcHandle hnvdc;
    struct nvdcFlipWinArgs flipWin; // to hold the window dimension
#endif

    NvColorFormat eColorFormatType;
} NvxOverlay;

NvError NvxAllocateOverlay(
    NvU32 *pWidth,  // [in/out]
    NvU32 *pHeight, // [in/out]
    NvColorFormat eFormat,
    NvxOverlay *pOverlay,
    ENvxVidRendType eType,
    NvBool bDisplayAtRequestRect,
    NvRect *pDisplayRect,
    NvBool bColorKey,
    NvU32  nColorKey,
    ENvxBlendType eBlendType,
    NvU32  nBlendAlphaValue,
    NvU32  nRotation,
    NvU32  nRealRotation,
    NvBool bKeepAspect,
    NvU32  nIndex,
    NvU32  nDepth,
    NvBool bNoAVSync,
    NvBool bTurnOffWinA,
    NvU32  nLayout,
    NvBool bForce2DOverride,
    NvUPtr pPassedInANW,
    NvUPtr pPassedInAndroidCameraPreview);

void NvxReleaseOverlay(NvxOverlay *pOverlay);
void NvxRenderSurfaceToOverlay(NvxOverlay *pOverlay, NvxSurface *pSrcSurface,
                               OMX_BOOL bWait);
void NvxUpdateOverlay(NvRect *pNewDestRect, NvxOverlay *pOverlay);

void NvxAdjustOverlayAspect(NvxOverlay *pOverlay);

void ClearYUVSurface(NvxSurface *pSurface);

OMX_U32 NvxGetOsRotation(void);

void NvxSmartDimmerEnable(NvxOverlay *pOverlay, NvBool enable);

// This function assumes both buffers are already allocated.
// This helper simply helps format the data and does any needed
// intermediate processing
NvError NvxCopyOMXSurfaceToMMBuffer(
    OMX_BUFFERHEADERTYPE *pOmxBuffer,
    NvMMBuffer *pMMBuffer,
    NvBool bSourceTiled);

NvError NvxCopyMMBufferToOMXBuf(
    NvMMBuffer *pMMBuf,
    NvMMBuffer *pScratchBuf,
    OMX_BUFFERHEADERTYPE *pOMXBuf);

NvError NvxCopyOMXBufferToNvxSurface(
    OMX_BUFFERHEADERTYPE *pOmxBuffer,
    NvxSurface *pSurface,
    NvU32 Width[],
    NvU32 Height[],
    NvS32 Count,
    NvBool bSourceTiled);

NvError NvxCopyOMXSurfaceToNvxSurface(
    OMX_BUFFERHEADERTYPE *pOmxBuffer,
    NvxSurface *pSurface,
    NvBool bSourceTiled);

NvError NvxCopyNvxSurfaceToOMXBuf(
    NvxSurface *pSurface,
    OMX_BUFFERHEADERTYPE *pOMXBuf);

void ConvertRect2Fx(NvRect *pNvRect, NvDdk2dFixedRect *pDdk2dFixedRect);

OMX_ERRORTYPE NvxCopyMMSurfaceDescriptor(
    NvMMSurfaceDescriptor *pDest, NvRect DstRect,
    NvMMSurfaceDescriptor *pSrc, NvRect SrcRect);

OMX_ERRORTYPE NvxCopyMMSurfaceDescriptor2DProc(NvMMSurfaceDescriptor *pDst, NvRect DstRect,
    NvMMSurfaceDescriptor *pSrc, NvRect SrcRect, NVX_CONFIG_VIDEO2DPROCESSING *pVPP,
    NvBool bApplyBackground);

OMX_ERRORTYPE NvxTranslateErrorCode(NvError eError);
OMX_ERRORTYPE NvxStereoRenderingEnable(NvxOverlay *pOverlay);

NvU32 ExtractStereoFlags(NvU32 bufferFlags);
NvStereoType NvMmStereoFlagsToNvRmStereoType(NvU32 stereoFlags);
NvU32 NvRmStereoTypeToNvMmStereoFlags(NvStereoType stereoType);
NvStereoType NvStereoTypeFromBufferFlags(NvU32 bufferFlags);

NvDdk2dTransform ConvertRotationMirror2Transform(OMX_S32 Rotation, OMX_MIRRORTYPE Mirror);

#endif /* __NVX_GFSDK_HELPERS_H__ */

