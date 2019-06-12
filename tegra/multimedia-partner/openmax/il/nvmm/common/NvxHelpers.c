/* Copyright (c) 2006-2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */
#include <math.h>

#include "NvxMutex.h"
#include "NvxScheduler.h"
#include "common/NvxTrace.h"
#include "NvxHelpers.h"

#include "nvos.h"
#include "nvmm_buffertype.h"
#include "nvmm_mediaclock.h"
#include "nvassert.h"
#include "nvmm_util.h"
#include "nvmm_exif.h"

#define EXCHANGE(a, b)  { NvU32 tmp; tmp = a; a = b; b = tmp; }

static OMX_HANDLETYPE oAcqLock = NULL;

static NvRmDeviceHandle s_hRmDevice = NULL;

static NvDdk2dHandle s_h2dHandle = NULL;
static OMX_HANDLETYPE s_2dLock = NULL;

#include "nvxhelpers_int.h"

static void ConvertBLtoPLBuffer(NvMMSurfaceDescriptor* pSrcSurface, NvMMSurfaceDescriptor* pOutSurf)
{
    NvError Err;
    NvDdk2dSurface *pSrcDdk2dSurface = NULL;
    NvDdk2dSurface *pDstDdk2dSurface = NULL;
    NvDdk2dHandle h2d = NvxGet2d();
    NvDdk2dSurfaceType Type1;
    NvDdk2dSurfaceType Type2;

    if (!h2d)
        return;

    if ((pSrcSurface->Surfaces[1].ColorFormat == NvColorFormat_U8_V8)
       || (pSrcSurface->Surfaces[1].ColorFormat == NvColorFormat_V8_U8))
       Type1 = NvDdk2dSurfaceType_Y_UV;
    else
       Type1 = NvDdk2dSurfaceType_Y_U_V;

    if ((pOutSurf->Surfaces[1].ColorFormat == NvColorFormat_U8_V8)
       || (pOutSurf->Surfaces[1].ColorFormat == NvColorFormat_V8_U8))
       Type2 = NvDdk2dSurfaceType_Y_UV;
    else
       Type2 = NvDdk2dSurfaceType_Y_U_V;

    NvxLock2d();
    Err = NvDdk2dSurfaceCreate(h2d, Type1,
                               &(pSrcSurface->Surfaces[0]), &pSrcDdk2dSurface);
    if (NvSuccess != Err)
    {
        goto L_cleanup;
    }
    Err = NvDdk2dSurfaceCreate(h2d, Type2,
                                &(pOutSurf->Surfaces[0]), &pDstDdk2dSurface);
    if (NvSuccess != Err)
    {
        goto L_cleanup;
    }


    Err = NvDdk2dBlit(h2d, pDstDdk2dSurface, NULL,
                      pSrcDdk2dSurface, NULL, NULL);
    if (NvSuccess != Err)
    {
        goto L_cleanup;
    }
    // Lock unlock to make sure 2D is done
    NvDdk2dSurfaceLock(pDstDdk2dSurface,
                       NvDdk2dSurfaceAccessMode_ReadWrite,
                       NULL,
                       NULL,
                       NULL);
    NvDdk2dSurfaceUnlock(pDstDdk2dSurface, NULL, 0);

L_cleanup:
    if (pSrcDdk2dSurface)
        NvDdk2dSurfaceDestroy(pSrcDdk2dSurface);
    if (pDstDdk2dSurface)
         NvDdk2dSurfaceDestroy(pDstDdk2dSurface);

    NvxUnlock2d();
}

/**
 * Convert omx's mirror and rotation to an NvDdk2dTransform used by DZ
 * Rotation is applied before mirroring.
 */
NvDdk2dTransform
ConvertRotationMirror2Transform(
    OMX_S32 Rotation,
    OMX_MIRRORTYPE Mirror)
{
    NvDdk2dTransform TransformSet[2][4] =
        {{ NvDdk2dTransform_None, NvDdk2dTransform_Rotate90,
           NvDdk2dTransform_Rotate180, NvDdk2dTransform_Rotate270},
         { NvDdk2dTransform_FlipVertical, NvDdk2dTransform_Transpose,
           NvDdk2dTransform_FlipHorizontal, NvDdk2dTransform_InvTranspose}};

    NvU32 i = 0, j = 0;

    switch (Mirror)
    {
        case OMX_MirrorVertical:
            i = 1;
            j = 0;
            break;

        case OMX_MirrorHorizontal:
            i = 1;
            j = 2;
            break;

        case OMX_MirrorBoth:
            i = 0;
            j = 2;
            break;

        case OMX_MirrorNone:
        default:
            i = 0;
            j = 0;
            break;
    }

    switch (Rotation)
    {
        case 90:
            j += 1;
            break;

        case 180:
            j += 2;
            break;

        case 270:
            j += 3;
            break;

        case 0:
        default:
            break;
    }

    return TransformSet[i%4][j%4];
}

OMX_ERRORTYPE NvxCreateAcqLock(void)
{
    if (!oAcqLock)
    {
        return NvxMutexCreate(&oAcqLock);
    }
    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxDestroyAcqLock(void)
{
    if (oAcqLock)
    {
        OMX_ERRORTYPE err = NvxMutexDestroy(oAcqLock);
        if (OMX_ErrorNone == err)
            oAcqLock = NULL;
        return err;
    }
    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxLockAcqLock(void)
{
    if (oAcqLock)
        return NvxMutexLock(oAcqLock);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxUnlockAcqLock(void)
{
    if (oAcqLock)
        return NvxMutexUnlock(oAcqLock);
    return OMX_ErrorNone;
}

// Not threadsafe - can't depend on threads existing at this point.
OMX_ERRORTYPE NvxInitPlatform(void)
{
    NvxCreateAcqLock();
    NvxMutexCreate(&s_2dLock);
    NvMMInitMediaClocks();
#if USE_ANDROID_CAMERA_PREVIEW
    NvxInitAndroidCameraPreview();
#endif

    return OMX_ErrorNone;
}

void NvxDeinitPlatform(void)
{
    NvxSchedulerShutdown(100);
    NvxDestroyAcqLock();

#if USE_ANDROID_CAMERA_PREVIEW
    NvxShutdownAndroidCameraPreview();
#endif

    if (s_h2dHandle)
    {
        NvDdk2dClose(s_h2dHandle);
        s_h2dHandle = NULL;
    }
    if (s_hRmDevice)
    {
        NvRmClose(s_hRmDevice);
        s_hRmDevice = NULL;
    }
    NvxMutexDestroy(s_2dLock);
    s_2dLock = NULL;

    NvMMDeInitMediaClocks();
}

NvRmDeviceHandle NvxGetRmDevice()
{
    NvError err;
    if(s_hRmDevice)
        return s_hRmDevice;

    err = NvRmOpen( &s_hRmDevice, 0);
    if (NvSuccess != err)
        return NULL;

    return s_hRmDevice;
}

NvDdk2dHandle NvxGet2d(void)
{
    NvError err;
    NvRmDeviceHandle hRmHandle;

    if (s_h2dHandle)
        return s_h2dHandle;

    hRmHandle = NvxGetRmDevice();
    if (!hRmHandle)
        return NULL;

    err = NvDdk2dOpen(hRmHandle, NULL, &s_h2dHandle);
    if (NvSuccess != err)
        return NULL;

    return s_h2dHandle;
}

void NvxLock2d(void)
{
    NvxMutexLock(s_2dLock);
}

void NvxUnlock2d(void)
{
    NvxMutexUnlock(s_2dLock);
}

void Nvx2dFlush(NvDdk2dSurface *pDstDdk2dSurface)
{
    if (!pDstDdk2dSurface)
        return;

    NvDdk2dSurfaceLock(pDstDdk2dSurface,
                       NvDdk2dSurfaceAccessMode_ReadWrite,
                       NULL,
                       NULL,
                       NULL);

    NvDdk2dSurfaceUnlock(pDstDdk2dSurface, NULL, 0);
}

NvxSurface *NvxGetPrimarySurface()
{
    return NULL;
}

/* From NvRM test lib sample */
NvError NvxAllocateMemoryBuffer(
    NvRmDeviceHandle hRmDev,
    NvRmMemHandle *hMem,
    NvU32 size,
    NvU32 align,
    NvU32 *phyAddress,
    NvOsMemAttribute Coherency,
    NvU16  Kind)
{
    NvError err = NvSuccess;
    NvRmMemCompressionTags compression_tag = NvRmMemCompressionTags_None;

    err = NvRmMemHandleCreate(hRmDev, hMem, size);
    if(err!= NvSuccess)
    {
        goto exit;
    }
    err = NvRmMemAllocBlocklinear(*hMem, NULL, 0, align, Coherency, Kind, compression_tag);
    if(err != NvSuccess)
    {
        NvRmMemHandleFree(*hMem);
        *hMem = NULL;
        goto exit;
    }
    *phyAddress = NvRmMemPin(*hMem);
exit:
    return err;
}

void NvxMemoryFree(NvRmMemHandle *hMem)
{
    NvRmMemUnpin(*hMem);
    NvRmMemHandleFree(*hMem);
    *hMem = NULL;
    return;
}

void ClearYUVSurface(NvxSurface *pSurface)
{
    int ysize, usize;
    NvU8 *pBuffer = NULL, *pmem;
    NvError err = NvSuccess;
    ysize = NvRmSurfaceComputeSize(&pSurface->Surfaces[0]);
    usize = NvRmSurfaceComputeSize(&pSurface->Surfaces[1]);

    err = NvRmMemMap(pSurface->Surfaces[0].hMem, pSurface->Surfaces[0].Offset,
                     ysize, NVOS_MEM_READ_WRITE, (void **)&pmem);
    if (err == NvSuccess)
    {
        NvOsMemset(pmem, 0x0, ysize);
        NvRmMemUnmap(pSurface->Surfaces[0].hMem, pmem, ysize);
    }
    else
    {
        pBuffer = NvOsAlloc(ysize);
        if (pBuffer)
        {

            NvOsMemset(pBuffer, 0x0, ysize);
            NvRmMemWrite(pSurface->Surfaces[0].hMem,
                         pSurface->Surfaces[0].Offset,
                         pBuffer, ysize);
            NvOsFree(pBuffer);
            pBuffer = NULL;
        }
    }

    err = NvRmMemMap(pSurface->Surfaces[1].hMem, pSurface->Surfaces[1].Offset,
                     usize, NVOS_MEM_READ_WRITE, (void **)&pmem);
    if (err == NvSuccess)
    {
        NvOsMemset(pmem, 0x7F, usize);
        NvRmMemUnmap(pSurface->Surfaces[1].hMem, pmem, usize);
    }
    else
    {
        pBuffer = NvOsAlloc(usize);
        if (pBuffer)
        {
            NvOsMemset(pBuffer, 0x7F, usize);
            NvRmMemWrite(pSurface->Surfaces[1].hMem,
                         pSurface->Surfaces[1].Offset,
                         pBuffer, usize);
            NvOsFree(pBuffer);
            pBuffer = NULL;
        }
    }

    if (pSurface->SurfaceCount == 3)
    {
        err = NvRmMemMap(pSurface->Surfaces[2].hMem, pSurface->Surfaces[2].Offset,
                usize, NVOS_MEM_READ_WRITE, (void **)&pmem);
        if (err == NvSuccess)
        {
            NvOsMemset(pmem, 0x7F, usize);
            NvRmMemUnmap(pSurface->Surfaces[2].hMem, pmem, usize);
        }
        else
        {
            pBuffer = NvOsAlloc(usize);
            if (pBuffer)
            {
                NvOsMemset(pBuffer, 0x7F, usize);
                NvRmMemWrite(pSurface->Surfaces[2].hMem,
                        pSurface->Surfaces[2].Offset,
                        pBuffer, usize);
                NvOsFree(pBuffer);
                pBuffer = NULL;
            }
        }
    }
}

#ifdef NV_DEF_USE_PITCH_MODE

#define NVRM_3D_SURFACE_YUV_PLANAR_ALIGN       1024
#define NVRM_3D_SURFACE_LINEAR_PITCH_ALIGNMENT 64

void NvRmSurfaceComputePitchTest(
    NvRmDeviceHandle hDevice,
    NvU32 flags,
    NvRmSurface *pSurf);

void NvRmSurfaceComputePitchTest(
    NvRmDeviceHandle hDevice,
    NvU32 flags,
    NvRmSurface *pSurf)
{
    NvU32 Pitch;

    NV_ASSERT(pSurf);
    /* Calculate Pitch & align (by adding pad bytes) */
    if (pSurf->Layout == NvRmSurfaceLayout_Pitch)
    {
        Pitch = pSurf->Width * NV_COLOR_GET_BPP(pSurf->ColorFormat);
        Pitch = (Pitch + 7) >> 3;
        Pitch = (Pitch + (NVRM_3D_SURFACE_LINEAR_PITCH_ALIGNMENT - 1)) &
                        ~(NVRM_3D_SURFACE_LINEAR_PITCH_ALIGNMENT - 1);
        pSurf->Pitch = Pitch;
    }
    else
    {
        /* No support for Tiled layout in 3D based compositing of video data */
        NV_ASSERT(0);
    }
    return;
}
#endif

NvError NvxSurfaceAllocYuv(
    NvxSurface *pSurface,
    NvU32 Width,
    NvU32 Height,
    NvU32 Format, // NvMM color format. Not Ddk color format
    NvU32 Layout,
    NvU32 *pImageSize,
    NvBool UseAlignment,
    NvOsMemAttribute Coherency,
    NvBool UseNV21)
{
    NvError err;
    NvRmDeviceHandle hRmDev = NvxGetRmDevice();
    NvU32 ComponentSize, SurfaceAlignment, index, ChromaHeight;
    NvMMSurfaceDescriptor *pSurfaceDesc = pSurface;
    NvU32 attrs[] = { NvRmSurfaceAttribute_Layout, Layout,
                      NvRmSurfaceAttribute_None };
    NvColorFormat PlaneFormat[3];
    NvYuvColorFormat yuvcolorformat= NvYuvColorFormat_Unspecified;

    pSurfaceDesc->Surfaces[0].hMem = NULL;
    pSurfaceDesc->Surfaces[1].hMem = NULL;
    pSurfaceDesc->Surfaces[2].hMem = NULL;

    *pImageSize = 0;

    switch (Format)
    {
    case NvMMVideoDecColorFormat_GRAYSCALE:
        // Even for Monochrome images HW expects vaild output surfaces for Chroma U and chroma V of YUV 444 type
        yuvcolorformat = NvYuvColorFormat_YUV444;
        break;

    case NvMMVideoDecColorFormat_YUV420:
        Width = (Width + 1) & ~1;
        Height = (Height + 1) & ~1;
        ChromaHeight = Height >> 1;

        if (Width < 32)
            Width = 32;
        // align to multiple of 16 for fast rotation support
        if (UseAlignment)
        {
            ChromaHeight += 15;
            ChromaHeight &= ~0x0F;
            Height = 2*ChromaHeight;
        }
        yuvcolorformat = NvYuvColorFormat_YUV420;
        break;

    case NvMMVideoDecColorFormat_YUV420SemiPlanar:
        Width = (Width + 1) & ~1;
        Height = (Height + 1) & ~1;

        if (Width < 32)
            Width = 32;
        yuvcolorformat = NvYuvColorFormat_YUV420;
        break;

    case NvMMVideoDecColorFormat_YUV422:
        yuvcolorformat = NvYuvColorFormat_YUV422;
        break;

    case NvMMVideoDecColorFormat_YUV422T:
        yuvcolorformat = NvYuvColorFormat_YUV422R;
        break;

    case NvMMVideoDecColorFormat_YUV422SemiPlanar:
        yuvcolorformat = NvYuvColorFormat_YUV422;
        break;

    case NvMMVideoDecColorFormat_YUV444:
        if(Width <16)
           Width = 16;
        yuvcolorformat = NvYuvColorFormat_YUV444;
        break;

    default:
        NV_DEBUG_PRINTF(("Unsupported color format\n"));
        break;
    }

    // NvRmMultiplanarSurfaceSetup asserts for odd height/width
    Width = (Width + 1) & ~1;
    Height = (Height + 1) & ~1;

    // Y surface
    PlaneFormat[0] = NvColorFormat_Y8;

    // U or UV surface
    if (NvMMVideoDecColorFormat_YUV420SemiPlanar == Format ||
        NvMMVideoDecColorFormat_YUV422SemiPlanar == Format)
    {
        if (UseNV21) {
            PlaneFormat[1] = NvColorFormat_V8_U8;
        } else {
            PlaneFormat[1] = NvColorFormat_U8_V8;
        }
        pSurfaceDesc->SurfaceCount = 2;
    }
    else
    {
        PlaneFormat[1] = NvColorFormat_U8;
        // V surface
        PlaneFormat[2] = NvColorFormat_V8;
        pSurfaceDesc->SurfaceCount = 3;
    }

    NvRmMultiplanarSurfaceSetup(pSurfaceDesc->Surfaces, pSurfaceDesc->SurfaceCount,
                                Width, Height, yuvcolorformat, PlaneFormat, attrs);
    for (index = 0; index < pSurfaceDesc->SurfaceCount; index++)
    {
#ifdef NV_DEF_USE_PITCH_MODE
        NvRmSurfaceComputePitchTest(NULL, 0, &pSurfaceDesc->Surfaces[index]);
        SurfaceAlignment = NVRM_3D_SURFACE_YUV_PLANAR_ALIGN;
#else
        SurfaceAlignment = NvRmSurfaceComputeAlignment(hRmDev, &pSurfaceDesc->Surfaces[index]);
#endif
        ComponentSize = NvRmSurfaceComputeSize(&pSurfaceDesc->Surfaces[index]);
        err = NvxAllocateMemoryBuffer(hRmDev, &pSurfaceDesc->Surfaces[index].hMem,
                                      ComponentSize, SurfaceAlignment,
                                      &pSurfaceDesc->PhysicalAddress[index],Coherency,
                                      pSurfaceDesc->Surfaces[index].Kind);
        if (err != NvSuccess)
        {
            NV_DEBUG_PRINTF(("Error in memory allocation for U\n"));
            goto nvx_error_bail;
        }
        *pImageSize += ComponentSize;
    }

    ClearYUVSurface(pSurface);
    NvOsMemset(&pSurfaceDesc->CropRect, 0, sizeof(NvRect));
    return NvSuccess;

nvx_error_bail:

    {
        NvRmSurface *pYSurf = &pSurfaceDesc->Surfaces[0];
        NvRmSurface *pUSurf = &pSurfaceDesc->Surfaces[1];
        NvRmSurface *pVSurf = &pSurfaceDesc->Surfaces[2];
        if (pYSurf)
            NvxMemoryFree(&pYSurf->hMem);
        if (pUSurf)
            NvxMemoryFree(&pUSurf->hMem);
        if (pVSurf)
            NvxMemoryFree(&pVSurf->hMem);
    }

    return NvError_InsufficientMemory;

}

NvError NvxSurfaceAllocContiguousYuv(
    NvxSurface *pSurface,
    NvU32 Width,
    NvU32 Height,
    NvU32 Format, // NvMM color format. Not Ddk color format
    NvU32 Layout,
    NvU32 *pImageSize,
    NvBool UseAlignment,
    NvOsMemAttribute Coherency,
    NvBool ClrYUVsurface)
{
    NvError err = NvSuccess;
    NvRmDeviceHandle hRmDev = NvxGetRmDevice();
    NvU32  ChromaHeight = 0, ComponentSize, SurfaceAlignment, align, size, index;
    NvMMSurfaceDescriptor *pSurfaceDesc = pSurface;
    NvU32 attrs[] = { NvRmSurfaceAttribute_Layout, Layout,
                      NvRmSurfaceAttribute_None };
    NvColorFormat PlaneFormat[3];
    NvYuvColorFormat yuvcolorformat= NvYuvColorFormat_Unspecified;

    pSurfaceDesc->Surfaces[0].hMem = NULL;
    pSurfaceDesc->Surfaces[1].hMem = NULL;
    pSurfaceDesc->Surfaces[2].hMem = NULL;

    *pImageSize = 0;

    switch (Format)
    {
    case NvMMVideoDecColorFormat_YUV420:
        Width = (Width + 1) & ~1;
        Height = (Height + 1) & ~1;
        ChromaHeight = Height >> 1;

        if (Width < 32)
            Width = 32;

        // align to multiple of 16 for fast rotation support
        if (UseAlignment)
        {
            ChromaHeight += 15;
            ChromaHeight &= ~0x0F;
            Height = 2*ChromaHeight;
        }
        yuvcolorformat = NvYuvColorFormat_YUV420;
        break;

    default:
        NV_DEBUG_PRINTF(("Unsupported color format\n"));
        break;
    }

    pSurfaceDesc->SurfaceCount = 3;
    // Y surface
    PlaneFormat[0] = NvColorFormat_Y8;
    // U surface
    PlaneFormat[1] = NvColorFormat_U8;
    // V surface
    PlaneFormat[2] = NvColorFormat_V8;
    NvRmMultiplanarSurfaceSetup((pSurfaceDesc->Surfaces), pSurfaceDesc->SurfaceCount,
                                 Width, Height, yuvcolorformat, PlaneFormat, attrs);


#ifdef NV_DEF_USE_PITCH_MODE
    NvRmSurfaceComputePitchTest(NULL, 0, &pSurfaceDesc->Surfaces[0]);
    SurfaceAlignment = NVRM_3D_SURFACE_YUV_PLANAR_ALIGN;
#else
    SurfaceAlignment = NvRmSurfaceComputeAlignment(hRmDev, &pSurfaceDesc->Surfaces[0]);
#endif
    ComponentSize = NvRmSurfaceComputeSize(&pSurfaceDesc->Surfaces[0]);

    for (index = 1; index < pSurfaceDesc->SurfaceCount; index ++)
    {
        size = NvRmSurfaceComputeSize(&pSurfaceDesc->Surfaces[index]);
#ifdef NV_DEF_USE_PITCH_MODE
        NvRmSurfaceComputePitchTest(NULL, 0, &pSurfaceDesc->Surfaces[index]);
        align = NVRM_3D_SURFACE_YUV_PLANAR_ALIGN;
#else
        align  = NvRmSurfaceComputeAlignment(hRmDev, pSurfaceDesc->Surfaces);
#endif
        NV_ASSERT(align <= SurfaceAlignment);
        // Align the surface offset
        pSurfaceDesc->Surfaces[index].Offset = (ComponentSize + align-1) & ~(align-1);
        ComponentSize = pSurfaceDesc->Surfaces[index].Offset + size;
    }
    err = NvxAllocateMemoryBuffer(hRmDev, &pSurfaceDesc->Surfaces[0].hMem,
                                  ComponentSize, SurfaceAlignment,
                                  &pSurfaceDesc->PhysicalAddress[0],Coherency,
                                  pSurfaceDesc->Surfaces[0].Kind);
    *pImageSize += ComponentSize;
    if (err != NvSuccess)
    {
        NV_DEBUG_PRINTF(("Error in memory allocation for Y\n"));
        goto nvx_error_bail;
    }

    for (index = 1; index < pSurfaceDesc->SurfaceCount; index++)
    {
        pSurfaceDesc->Surfaces[index].hMem = pSurfaceDesc->Surfaces[0].hMem;
    }

    if (ClrYUVsurface == NV_TRUE)
        ClearYUVSurface(pSurface);

    NvOsMemset(&pSurfaceDesc->CropRect, 0, sizeof(NvRect));
    return NvSuccess;

nvx_error_bail:

    {
        NvRmSurface *pYSurf = &pSurfaceDesc->Surfaces[0];
        if (pYSurf)
            NvxMemoryFree(&pYSurf->hMem);
    }

    return NvError_InsufficientMemory;
}

static NvError nvxSurfaceContiguousAlloc(
    NvxSurface **ppSurfHandle,
    NvU32 Width,
    NvU32 Height,
    NvColorFormat Format,
    NvU32 Layout,
    NvU32 *ImageSize,
    NvBool UseAlignment)
{
    NvError err = NvSuccess;
    NvRmDeviceHandle hRmDev = NvxGetRmDevice();
    NvxSurface *pSurface = NULL;

    if (!hRmDev)
        return NvError_NotInitialized;

    pSurface = NvOsAlloc(sizeof(NvxSurface));
    if (!pSurface)
    {
        *ppSurfHandle = NULL;
        return NvError_InsufficientMemory;
    }

    *ppSurfHandle = pSurface;
    NvOsMemset(pSurface, 0, sizeof(NvxSurface));

    // Set NvRmSurface values
    switch (Format)
    {
        case NvColorFormat_Y8 :
        {
            err = NvxSurfaceAllocContiguousYuv(
                pSurface,
                Width,
                Height,
                NvMMVideoDecColorFormat_YUV420,
                Layout,
                ImageSize,
                UseAlignment,NvOsMemAttribute_Uncached,
                NV_TRUE);

            if (err != NvSuccess)
            {
                NvRmSurface *pYSurf = &pSurface->Surfaces[0];
                if (pYSurf)
                    NvxMemoryFree(&pYSurf->hMem);
                NvOsFree(pSurface);
                return err;
            }
        }
        break;

        // Default to RGB format
        default:
        {
            NvOsFree(pSurface);
            *ppSurfHandle = NULL;
        }
        break;
    }

    return err;
}

NvError NvxSurfaceAllocContiguous(
    NvxSurface **ppSurf,
    NvU32 Width,
    NvU32 Height,
    NvColorFormat Format,
    NvU32 Layout)
{
    NvU32 size = 0;
    NvError res = NvSuccess;

    res = nvxSurfaceContiguousAlloc(ppSurf,Width,Height,Format,Layout,&size,NV_TRUE);
    return res;
}

OMX_U8* NvxAllocRmSurf(OMX_U32 *nSize,
    NvU32 Width,
    NvU32 Height,
    NvColorFormat Format,
    NvU32 Layout,
    NvxSurface **ppSurfHandle)
{
    NvError err = NvSuccess;
    OMX_U8 *pData = NULL;

    err = nvxSurfaceContiguousAlloc(
            ppSurfHandle,
            Width,
            Height,
            NvColorFormat_Y8,
            Layout,
            (NvU32 *)nSize,
            NV_FALSE);
    if (NvSuccess != err)
    {
        return NULL;
    }
    else
    {
        NvMMSurfaceDescriptor *pSurfaceDesc = *ppSurfHandle;
        err = NvRmMemMap(pSurfaceDesc->Surfaces[0].hMem ,
            pSurfaceDesc->Surfaces[0].Offset,
            (NvU32)*nSize,
            NVOS_MEM_READ_WRITE,
            (void **)&pData);
        if (pData == NULL)
        {
            NvRmSurface *pYSurf = &(*ppSurfHandle)->Surfaces[0];
            if (pYSurf)
                NvxMemoryFree(&pYSurf->hMem);
            NvOsFree(*ppSurfHandle);
            NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "%s[%d] memmap Failed() for pSurfaceDesc %p \n",
                __FUNCTION__,__LINE__, pSurfaceDesc));
            return NULL;
        }
        else
        {
            OMX_U32 *pRmSurf = (OMX_U32 *)(pData + NvOsStrlen("NVNVRMSURFACENV") + 1);
            NvOsStrncpy((char *)pData, "NVNVRMSURFACENV", NvOsStrlen("NVNVRMSURFACENV"));
            *pRmSurf = (OMX_U32)pSurfaceDesc;
            return pData;
        }
    }
}

void NvxFreeRmSurf(OMX_U8 *pData, NvU32 nSize)
{
    OMX_U32 RmSurf = *((NvU32 *)(pData + NvOsStrlen("NVNVRMSURFACENV") + 1));
    NvxSurface *ppSurfHandle = NULL;
    NvMMSurfaceDescriptor *pSurfaceDesc = NULL;

    if (!NvOsStrncmp((char *)pData, "NVNVRMSURFACENV", NvOsStrlen("NVNVRMSURFACENV")))
    {
        NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "%s[%d] free surf desc (%p) pbuffer(%p)\n",
                    __FUNCTION__, __LINE__, RmSurf , pData));
        ppSurfHandle = (NvxSurface *)RmSurf;
        pSurfaceDesc = ppSurfHandle;
        {
            NvRmSurface *pYSurf = &pSurfaceDesc->Surfaces[0];
            if (pYSurf)
            {
                NvxMemoryFree(&pYSurf->hMem);
            }
        }
        NvRmMemUnmap(pSurfaceDesc->Surfaces[0].hMem, pData, nSize);

        NvOsFree(pSurfaceDesc);
    }
    else
    {
        NVXTRACE((NVXT_ERROR, NVXT_UNDESIGNATED, "%s[%d] NVNVRMSURFACENV(%p) tag for pBuffer %p not found\n",
            __FUNCTION__, __LINE__, RmSurf, pData));
    }
    return;
}

/* From NvRM test lib sample */
NvError NvxSurfaceAlloc(
    NvxSurface **ppSurfHandle,
    NvU32 Width,
    NvU32 Height,
    NvColorFormat Format,
    NvU32 Layout)
{
    NvU32 size, align;
    NvError err = NvSuccess;
    NvRmDeviceHandle hRmDev = NvxGetRmDevice();
    NvxSurface *pSurface = NULL;
    NvU32 attrs[] = { NvRmSurfaceAttribute_Layout, Layout,
                      NvRmSurfaceAttribute_None };

    if (!hRmDev)
        return NvError_NotInitialized;

    pSurface = NvOsAlloc(sizeof(NvxSurface));
    if (!pSurface)
    {
        *ppSurfHandle = NULL;
        return NvError_InsufficientMemory;
    }

    *ppSurfHandle = pSurface;
    NvOsMemset(pSurface, 0, sizeof(NvxSurface));

    // Set NvRmSurface values
    switch (Format)
    {
        case NvColorFormat_Y8 :
        {
            NvU32 ImageSize = 0;
            err = NvxSurfaceAllocYuv(
                pSurface,
                Width,
                Height,
                NvMMVideoDecColorFormat_YUV420,
                Layout,
                &ImageSize,
                NV_TRUE,
                NvOsMemAttribute_Uncached,
                NV_FALSE);

            if (err != NvSuccess)
                return err;
        }
        break;

        // Default to RGB format
        default:
        {
            NvRmSurface *pSurf = &pSurface->Surfaces[0];
            NvU8 *pZeroBuffer = NULL;
            NvU32 address;

            pSurface->SurfaceCount = 1;
            attrs[1] = NvRmSurfaceLayout_Pitch;
            NvRmSurfaceSetup(pSurf, Width, Height, Format, attrs);
            size  = NvRmSurfaceComputeSize(pSurf);
            align = NvRmSurfaceComputeAlignment(hRmDev, pSurf);

            err = NvxAllocateMemoryBuffer(hRmDev, &pSurf->hMem, size, align,
                                          &address,NvOsMemAttribute_Uncached,
                                           pSurf[0].Kind);
            if(err != NvSuccess)
            {
                NvRmMemUnpin(*(&pSurf->hMem));
                NvRmMemHandleFree(*(&pSurf->hMem));
                NvOsMemset(pSurf, 0, sizeof(NvRmSurface));
                NvOsFree(pSurface);
                *ppSurfHandle = NULL;
                return err;
            }

            // Else, success. Clear frame data
            err = NvRmMemMap( pSurf->hMem, pSurf->Offset,
                size, NVOS_MEM_READ_WRITE, (void**)&pZeroBuffer);
            if (err == NvSuccess)
            {
                NvOsMemset( pZeroBuffer, 0x00, size);
                NvRmMemUnmap( pSurf->hMem, pZeroBuffer, size);
            }
        }
        break;
    }

    return err;
}

void NvxSurfaceFree(NvxSurface **ppSurf)
{
    NvMMSurfaceDescriptor *serfDescr = NULL;

    if (!ppSurf || !*ppSurf)
        return;

    serfDescr = (NvMMSurfaceDescriptor *)*ppSurf;
    NvMMUtilDestroySurfaces(serfDescr);

    NvOsFree(*ppSurf);
    *ppSurf = NULL;
}

void NvxAdjustOverlayAspect(NvxOverlay *pOverlay)
{
    float sourceAspect, destAspect;

    sourceAspect = (float)pOverlay->srcW / pOverlay->srcH;
    destAspect = (float)pOverlay->destW / pOverlay->destH;

    if (pOverlay->bKeepAspect &&
        (fabs(destAspect - sourceAspect) / NVX_MIN(destAspect, sourceAspect)) > 0.05)
    {
        if (destAspect > sourceAspect)
        {
            float pix = (float)(((sourceAspect / destAspect) * (float)pOverlay->destW) + 0.5);
            pOverlay->destX += (pOverlay->destW - (int)pix) / 2;
            pOverlay->destW = (int)pix;
        }
        else
        {
            float pix = (float)(((destAspect / sourceAspect) * (float)pOverlay->destH) + 0.5);
            pOverlay->destY += (pOverlay->destH - (int)pix) / 2;
            pOverlay->destH = (int)pix;
        }
    }
}

NvError NvxAllocateOverlay(
    NvU32 *pWidth,
    NvU32 *pHeight,
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
    NvUPtr pPassedInAndroidCameraPreview)
{
    NvError err = NvError_BadParameter;

    NvOsMemset(pOverlay, 0, sizeof(NvxOverlay));

    pOverlay->bColorKey = bColorKey;
    pOverlay->nColorKey = nColorKey;
    pOverlay->eBlendType = eBlendType;
    pOverlay->nBlendAlphaValue = nBlendAlphaValue;
    pOverlay->eType = eType;
    pOverlay->nRotation = nRotation;
    pOverlay->nSetRotation = nRealRotation;
    pOverlay->bKeepAspect = bKeepAspect;
    pOverlay->bNoAVSync = bNoAVSync;
    pOverlay->bTurnOffWinA = bTurnOffWinA;
    pOverlay->bForce2D = bForce2DOverride;
    pOverlay->bSmartDimmerOn = NV_FALSE;
    pOverlay->eColorFormatType = eFormat;
    pOverlay->nDepth = nDepth;
    pOverlay->nIndex = nIndex;

    if (NvSuccess != ((err = NvRmOpen(&pOverlay->hRmDev, 0))))
        return err;

#if USE_ANDROID_NATIVE_WINDOW
    if (pPassedInANW &&
        ((eFormat == NvColorFormat_Y8 && eType == Rend_TypeOverlay) ||
         (eFormat == NvColorFormat_Y8 && eType == Rend_TypeHDMI) ||
         (eFormat == NvColorFormat_Y8 && eType == Rend_TypeTVout) ||
         (eFormat == NvColorFormat_Y8 && eType == Rend_TypeSecondary)))
    {
        pOverlay->pANW = pPassedInANW;
        return NvxAllocateOverlayANW(pWidth, pHeight, eFormat, pOverlay,
                                     eType, bDisplayAtRequestRect,
                                     pDisplayRect, nLayout);
    }
#endif
#if USE_ANDROID_CAMERA_PREVIEW
    if (pPassedInAndroidCameraPreview &&
        ((eFormat == NvColorFormat_Y8 && eType == Rend_TypeOverlay) ||
         (eFormat == NvColorFormat_Y8 && eType == Rend_TypeHDMI) ||
         (eFormat == NvColorFormat_Y8 && eType == Rend_TypeTVout) ||
         (eFormat == NvColorFormat_Y8 && eType == Rend_TypeSecondary)))
    {
        pOverlay->pAndroidCameraPreview = pPassedInAndroidCameraPreview;
        return NvxAllocateOverlayAndroidCameraPreview(pWidth, pHeight, eFormat, pOverlay,
                                     eType, bDisplayAtRequestRect,
                                     pDisplayRect, nLayout);
    }
#endif

    if ((eFormat == NvColorFormat_Y8 || eFormat == NvColorFormat_A8R8G8B8) &&
        (eType == Rend_TypeOverlay ||
         eType == Rend_TypeHDMI ||
         eType == Rend_TypeLVDS ||
         eType == Rend_TypeCRT ||
         eType == Rend_TypeTVout ||
         eType == Rend_TypeSecondary))
    {
#ifdef USE_DC_DRIVER
        err = NvxAllocateOverlayDC(pWidth, pHeight, eFormat,
                                   pOverlay, eType,
                                   bDisplayAtRequestRect,
                                   pDisplayRect,
                                   nLayout);
        return err;
#endif
    }

    pOverlay->srcX = pOverlay->srcY = 0;
    pOverlay->srcW = *pWidth;
    pOverlay->srcH = *pHeight;
    NvxSurfaceAlloc(&pOverlay->pSurface, *pWidth, *pHeight, eFormat, nLayout);
    err = NvSuccess;

    return err;
}

void NvxReleaseOverlay(NvxOverlay *pOverlay)
{
#if USE_ANDROID_NATIVE_WINDOW
    if (pOverlay->pANW)
    {
        NvxReleaseOverlayANW(pOverlay);
        pOverlay->pANW = 0;
    }
#endif
#if USE_ANDROID_CAMERA_PREVIEW
    if (pOverlay->pAndroidCameraPreview)
    {
        NvxReleaseOverlayAndroidCameraPreview(pOverlay);
        pOverlay->pAndroidCameraPreview = 0;
    }
#endif
#ifdef USE_DC_DRIVER
    if (pOverlay->bUsingDC)
    {
        NvxReleaseOverlayDC(pOverlay);
        pOverlay->bUsingDC = OMX_FALSE;
    }
#endif

    if (pOverlay->pSurface)
        NvxSurfaceFree(&pOverlay->pSurface);
    pOverlay->pSurface = NULL;

    if (pOverlay->hRmDev)
        NvRmClose(pOverlay->hRmDev);

    pOverlay->hRmDev = NULL;
}

void NvxRenderSurfaceToOverlay(NvxOverlay *pOverlay, NvxSurface *pSrcSurface,
                               OMX_BOOL bWait)
{
    if (!pSrcSurface)
        return;

#if USE_ANDROID_NATIVE_WINDOW
    if (pOverlay->pANW)
    {
        NvxRenderSurfaceToOverlayANW(pOverlay, pSrcSurface, bWait);
        return;
    }
#endif
#if USE_ANDROID_CAMERA_PREVIEW
    if (pOverlay->pAndroidCameraPreview)
    {
        NvxRenderSurfaceToOverlayAndroidCameraPreview(pOverlay, pSrcSurface, bWait);
        return;
    }
#endif
#ifdef USE_DC_DRIVER
    if (pOverlay->bUsingDC)
    {
        NvxRenderSurfaceToOverlayDC(pOverlay, pSrcSurface, bWait);
    }
#endif
}

void NvxUpdateOverlay(NvRect *pNewDestRect, NvxOverlay *pOverlay)
{
#if USE_ANDROID_NATIVE_WINDOW
    if (pOverlay->pANW)
    {
        NvxUpdateOverlayANW(pNewDestRect, pOverlay, NV_FALSE);
        if (pOverlay->pSurface &&
            (pOverlay->pSurface->Surfaces[0].Width != pOverlay->srcW ||
             pOverlay->pSurface->Surfaces[0].Height != pOverlay->srcH))
        {
            NvColorFormat eFormat = pOverlay->pSurface->Surfaces[0].ColorFormat;
            NvU32 nLayout = pOverlay->pSurface->Surfaces[0].Layout;
            NvxSurfaceFree(&pOverlay->pSurface);
            pOverlay->pSurface = NULL;

            NvxSurfaceAlloc(&pOverlay->pSurface, pOverlay->srcW, pOverlay->srcH, eFormat, nLayout);
        }
    }
#endif
#if USE_ANDROID_CAMERA_PREVIEW
    if (pOverlay->pAndroidCameraPreview)
    {
        NvxUpdateOverlayAndroidCameraPreview(pNewDestRect, pOverlay, NV_FALSE);
    }
#endif
#ifdef USE_DC_DRIVER
    if(pOverlay->bUsingDC)
    {
        NvxUpdateOverlayDC(pNewDestRect, pOverlay, NV_FALSE);
    }
#endif
}

static NvError NvxSwapSurfaceUV(NvRmSurface *pSurf)
{
    NvError err;
    NvDdk2dSurface *pSrcDdk2dSurface = NULL;
    NvDdk2dSurface *pDstDdk2dSurface = NULL;
    NvDdk2dSurface *pDstTweakedDdk2dSurface = NULL;
    NvRmSurface RmSurf;
    NvDdk2dHandle h2d = NvxGet2d();
    NvU32 SurfaceAlignment, ComponentSize;
    NvDdk2dBlitParameters Param = {0};
    NvDdk2dFixedRect SrcRectLocal;
    NvRect SrcRect, DstRect;
    NvU32 PhyAddr;
    NvDdk2dColor SolidColor;
    NvU32 attrs[] = { NvRmSurfaceAttribute_Layout, pSurf->Layout,
                      NvRmSurfaceAttribute_None };

    if (!h2d)
        return OMX_ErrorBadParameter;

    // From now on, treat src surf. as 8bpp
    pSurf->Width <<= 1;
    pSurf->ColorFormat = NvColorFormat_L8;

    // Create Rm surf
    NvOsMemset(&RmSurf, 0, sizeof(NvRmSurface));
    NvRmSurfaceSetup(&(RmSurf), pSurf->Width + 4, pSurf->Height, NvColorFormat_L8, attrs);
    SurfaceAlignment = NvRmSurfaceComputeAlignment(s_hRmDevice, &RmSurf);
    ComponentSize = NvRmSurfaceComputeSize(&RmSurf);
    err = NvxAllocateMemoryBuffer(s_hRmDevice, &RmSurf.hMem, ComponentSize, SurfaceAlignment, &PhyAddr,NvOsMemAttribute_Uncached, RmSurf.Kind);
    if (err != NvSuccess)
    {
        NV_DEBUG_PRINTF(("Error in memory allocation for Y\n"));
        goto cleanup;
    }

    // Wrap around 2d surf
    NvxLock2d();

    err = NvDdk2dSurfaceCreate(h2d, NvDdk2dSurfaceType_Single, pSurf, &pSrcDdk2dSurface);
    if (NvSuccess != err)
        goto cleanup;

    err = NvDdk2dSurfaceCreate(h2d, NvDdk2dSurfaceType_Single, &RmSurf, &pDstDdk2dSurface);
    if (NvSuccess != err)
        goto cleanup;

    // Set BLT attributes
    NvDdk2dSetBlitFilter(&Param, NvDdk2dStretchFilter_Nearest);
    NvDdk2dSetBlitTransform(&Param, NvDdk2dTransform_None);

    // First BLT: (0, 0) => (1, 0)
    NvOsMemset(&SrcRect, 0, sizeof(NvRect));
    SrcRect.right = pSurf->Width;
    SrcRect.bottom = pSurf->Height;
    ConvertRect2Fx(&SrcRect, &SrcRectLocal);

    NvOsMemcpy(&DstRect, &SrcRect, sizeof(NvRect));
    DstRect.left = 1;
    DstRect.right++;

    err = NvDdk2dBlit(h2d, pDstDdk2dSurface, &DstRect, pSrcDdk2dSurface, &SrcRectLocal, &Param);
    if (NvSuccess != err)
        goto cleanup;
    Nvx2dFlush(pDstDdk2dSurface);

    // Second BLT: ROP 0xAC and treat as 16bpp surface
    RmSurf.ColorFormat = NvColorFormat_R5G6B5;
    RmSurf.Width >>= 1;
    err = NvDdk2dSurfaceCreate(h2d, NvDdk2dSurfaceType_Single, &RmSurf, &pDstTweakedDdk2dSurface);
    if (NvSuccess != err)
        goto cleanup;

    NvOsMemset(&SrcRect, 0, sizeof(NvRect));
    SrcRect.left = 1;
    SrcRect.right = RmSurf.Width + 1;
    SrcRect.bottom = RmSurf.Height;
    ConvertRect2Fx(&SrcRect, &SrcRectLocal);

    NvOsMemcpy(&DstRect, &SrcRect, sizeof(NvRect));
    DstRect.left = 0;
    DstRect.right = RmSurf.Width;

    // Solid brush needed
    NvDdk2dSetBlitRop3(&Param, 0xACUL);
    SolidColor.Format = NvColorFormat_R5G6B5;
    *((NvU32*)SolidColor.Value) = 0xFF00UL;
    NvDdk2dSetBlitBrush(&Param, NvDdk2dBrushType_SolidColor, NULL, NULL, NULL, &SolidColor, NULL);

    err = NvDdk2dBlit(h2d, pDstTweakedDdk2dSurface, &DstRect, pDstTweakedDdk2dSurface, &SrcRectLocal, &Param);
    if (NvSuccess != err)
        goto cleanup;
    Nvx2dFlush(pDstTweakedDdk2dSurface);

    // Copy back the result
    NvOsMemset(&SrcRect, 0, sizeof(NvRect));
    SrcRect.right = pSurf->Width;
    SrcRect.bottom = pSurf->Height;
    ConvertRect2Fx(&SrcRect, &SrcRectLocal);
    NvOsMemcpy(&DstRect, &SrcRect, sizeof(NvRect));
    NvDdk2dSetBlitRop3(&Param, 0xCCUL);
    err = NvDdk2dBlit(h2d, pSrcDdk2dSurface, &DstRect, pDstDdk2dSurface, &SrcRectLocal, &Param);
    if (NvSuccess != err)
        goto cleanup;
    Nvx2dFlush(pSrcDdk2dSurface);

cleanup:
    // Restore src surf. attr
    pSurf->Width >>= 1;
    pSurf->ColorFormat = NvColorFormat_U8_V8;

    if (pSrcDdk2dSurface)
        NvDdk2dSurfaceDestroy(pSrcDdk2dSurface);
    if (pDstDdk2dSurface)
        NvDdk2dSurfaceDestroy(pDstDdk2dSurface);
    if (pDstTweakedDdk2dSurface)
        NvDdk2dSurfaceDestroy(pDstTweakedDdk2dSurface);

    NvxMemoryFree(&RmSurf.hMem);
    NvxUnlock2d();

    return err;
}

NvError NvxCopyOMXBufferToNvxSurface(
    OMX_BUFFERHEADERTYPE *pOmxBuffer,
    NvxSurface *pSurface,
    NvU32 Width[],
    NvU32 Height[],
    NvS32 Count,
    NvBool bSourceTiled)
{
    NvError err = NvSuccess;
    OMX_U8 *pSrc = NULL;
    NvS32 index = 0;

    if (!pOmxBuffer || !pSurface)
        return NvError_BadParameter;

    NV_ASSERT(Count == pSurface->SurfaceCount);

    pSrc = pOmxBuffer->pBuffer;

    if ((2 == pSurface->SurfaceCount) &&
        (NvColorFormat_Y8 == pSurface->Surfaces[0].ColorFormat) &&
        (NvColorFormat_U8_V8 == pSurface->Surfaces[1].ColorFormat))
    {
        NvRmSurfaceWrite(&pSurface->Surfaces[0], 0, 0,
            Width[0],
            Height[0],
            pSrc);
        pSrc += (Width[0] * Height[0]);

        NvRmSurfaceWrite(&pSurface->Surfaces[1], 0, 0,
            Width[1],
            Height[1],
            pSrc);

        err = NvxSwapSurfaceUV(&pSurface->Surfaces[1]);
    }
    else
    {
        for (index = 0; index < pSurface->SurfaceCount; index++)
        {
            if (bSourceTiled)
            {
                // tiled surface input data
                NvRmMemWrite(pSurface->Surfaces[index].hMem,
                    pSurface->Surfaces[index].Offset,
                    pSrc,
                    (pSurface->Surfaces[index].Pitch * pSurface->Surfaces[index].Height));
            }
            else
            {
                // common case, where data is usually non-tiled, planar:
                NvRmSurfaceWrite(&(pSurface->Surfaces[index]), 0, 0,
                    Width[index],
                    Height[index],
                    pSrc);
            }

            if (bSourceTiled)
                pSrc += (pSurface->Surfaces[index].Pitch * pSurface->Surfaces[index].Height);
            else
                pSrc += (Width[index] * Height[index]);

            if (pSrc > pOmxBuffer->pBuffer + pOmxBuffer->nFilledLen)
            {
                break;
            }
        }
    }
    return err;
}

NvError NvxCopyOMXSurfaceToNvxSurface(
    OMX_BUFFERHEADERTYPE *pOmxBuffer,
    NvxSurface *pSurface,
    NvBool bSourceTiled)
{
    NvU32 Width[3], Height[3];
    int index = 0;

    if (!pSurface)
        return NvError_BadParameter;

    for (index = 0; index < pSurface->SurfaceCount; index++)
    {
        Width[index] = pSurface->Surfaces[index].Width;
        Height[index] = pSurface->Surfaces[index].Height;
    }
    return NvxCopyOMXBufferToNvxSurface(pOmxBuffer,
        pSurface,
        Width,
        Height,
        pSurface->SurfaceCount,
        bSourceTiled);
}


NvError NvxCopyOMXSurfaceToMMBuffer(
    OMX_BUFFERHEADERTYPE *pOmxBuffer,
    NvMMBuffer *pMMBuffer,
    NvBool bSourceTiled)
{
    NvxSurface *pNvMMSurface = NULL;
    OMX_OTHER_EXTRADATATYPE extraHeader, *pExtraHeader;
    NvU32 dataOffset = (NvU8*)&extraHeader.data - (NvU8*)&extraHeader;
    NvU32 zero = 0;
    NvU32 metadataSize, bufferSize, bufferOffset, exifSize = sizeof(NvMMEXIFInfo);
    NvU8 *extraBase, *metadataBase;
    NvRmMemHandle phMem;
    NvError err;

    if (!pOmxBuffer || !pMMBuffer)
        return NvError_BadParameter;

    pNvMMSurface = &pMMBuffer->Payload.Surfaces;

    if (!pNvMMSurface)
        return NvError_BadParameter;

    err = NvxCopyOMXSurfaceToNvxSurface(pOmxBuffer, pNvMMSurface, bSourceTiled);
    if (err != NvSuccess)
        return err;

    // Handle any NVRAW metadata buffer passed
    // First test if the output buffer can receive it

    // Not an NvMMBufferMetadataType_Camera metadata type?
    if (pMMBuffer->PayloadInfo.BufferMetaDataType != NvMMBufferMetadataType_Camera)
        return NvSuccess;

    // Metatdata memory handle allocated?
    phMem = pMMBuffer->PayloadInfo.BufferMetaData.CameraBufferMetadata.EXIFInfoMemHandle;
    if (phMem == NULL)
        return NvSuccess;

    // if there is metadata, at a bare minimum, it must
    // start with a magic number and a version field, both of which
    // are 32 bit unsigned integers.
    bufferSize = NvRmMemGetSize(phMem);
    if (bufferSize < (exifSize + 2 * sizeof(NvU32)))
    {
        return NvSuccess;
    }

    // Compute our starting point measuring from the end of the buffer
    // in case other stuff (such as exif) is at the front
    bufferOffset = exifSize;

    // Successful transfer of data will return success
    // Otherwise fall through to clean up
    if (pOmxBuffer->nFlags & OMX_BUFFERFLAG_EXTRADATA)
    {
        extraBase = pOmxBuffer->pBuffer + pOmxBuffer->nFilledLen;
        pExtraHeader = (OMX_OTHER_EXTRADATATYPE*) extraBase;
        metadataBase = extraBase + dataOffset;
        metadataSize = pExtraHeader->nDataSize;

        if (metadataSize + exifSize == bufferSize)
        {
            NvRmMemWrite(phMem, bufferOffset, metadataBase, metadataSize);
            return NvSuccess;
        }
        err = NvError_BadValue; // metadata size mismatch
    }

    // If there was no valid NVRAW metadata passed,
    // but space was allocated in the output buffer
    // then we have to invalidate it by zeroing the
    // magic number and version
    NvRmMemWrite(phMem, bufferOffset, &zero, sizeof(NvU32));
    NvRmMemWrite(phMem, bufferOffset + sizeof(NvU32), &zero, sizeof(NvU32));
    return err;
}

void ConvertRect2Fx(NvRect *pNvRect, NvDdk2dFixedRect *pDdk2dFixedRect)
{
    if (pNvRect && pDdk2dFixedRect)
    {
        pDdk2dFixedRect->left   = NV_SFX_WHOLE_TO_FX(pNvRect->left);
        pDdk2dFixedRect->top    = NV_SFX_WHOLE_TO_FX(pNvRect->top);
        pDdk2dFixedRect->right  = NV_SFX_WHOLE_TO_FX(pNvRect->right);
        pDdk2dFixedRect->bottom = NV_SFX_WHOLE_TO_FX(pNvRect->bottom);
    }
    return;
}

NvError NvxCopyNvxSurfaceToOMXBuf(NvxSurface *pSurface, OMX_BUFFERHEADERTYPE *pOMXBuf)
{
    NvU32 curlen, length = 0;
    int index;

    if (!pSurface || !pOMXBuf)
        return NvError_BadParameter;

    for (index = 0; index < pSurface->SurfaceCount; index++)
    {
        if (pSurface->Surfaces[index].hMem )
        {
            curlen = pSurface->Surfaces[index].Width * pSurface->Surfaces[index].Height;
            // Currently there is no function available to get the
            // surface's size of data excluding pitch padding.
            switch (pSurface->Surfaces[index].ColorFormat)
            {
            case NvColorFormat_A4R4G4B4:      curlen *= 2; break;
            case NvColorFormat_A1R5G5B5:      curlen *= 2; break;
            case NvColorFormat_R5G5B5A1:      curlen *= 2; break;
            case NvColorFormat_R5G6B5:        curlen *= 2; break;
            case NvColorFormat_X6Bayer10BGGR: curlen *= 2; break;
            case NvColorFormat_X6Bayer10RGGB: curlen *= 2; break;
            case NvColorFormat_X6Bayer10GRBG: curlen *= 2; break;
            case NvColorFormat_X6Bayer10GBRG: curlen *= 2; break;
            case NvColorFormat_R8_G8_B8:      curlen *= 3; break;
            case NvColorFormat_B8_G8_R8:      curlen *= 3; break;
            case NvColorFormat_A8R8G8B8:      curlen *= 4; break;
            case NvColorFormat_A8B8G8R8:      curlen *= 4; break;
            case NvColorFormat_R8G8B8A8:      curlen *= 4; break;
            case NvColorFormat_B8G8R8A8:      curlen *= 4; break;
            case NvColorFormat_U8_V8:         curlen *= 2; break;
            default: break;
            }

            if (length + curlen > pOMXBuf->nAllocLen)
            {
                NvOsDebugPrintf("buffer too small for surface \n");
                break;
            }
            NvRmSurfaceRead(
              &(pSurface->Surfaces[index]),
              0,
              0,
              pSurface->Surfaces[index].Width,
              pSurface->Surfaces[index].Height,
              pOMXBuf->pBuffer + length);

              length += curlen;
        }

    }

    pOMXBuf->nFilledLen = length;

    return NvSuccess;
}

NvError NvxCopyMMBufferToOMXBuf(NvMMBuffer *pMMBuf, NvMMBuffer *pScratchBuf,  OMX_BUFFERHEADERTYPE *pOMXBuf)
{
    NvMMSurfaceDescriptor *pSurfaceDesc = NULL;
    NvMMSurfaceDescriptor *pIntSurfaceDesc = NULL;

    if (!pMMBuf || !pOMXBuf)
        return NvError_BadParameter;

    pSurfaceDesc = &pMMBuf->Payload.Surfaces;
    if (pScratchBuf)
    {
         NV_DEBUG_PRINTF(("Doing a 2D blit for BL surface"));
         pIntSurfaceDesc = &pScratchBuf->Payload.Surfaces;
         ConvertBLtoPLBuffer(pSurfaceDesc, pIntSurfaceDesc);
    }
    else
    {
         pIntSurfaceDesc = pSurfaceDesc;
    }

    return NvxCopyNvxSurfaceToOMXBuf(pIntSurfaceDesc, pOMXBuf);
}

OMX_ERRORTYPE NvxCopyMMSurfaceDescriptor(NvMMSurfaceDescriptor *pDest, NvRect DstRect,
    NvMMSurfaceDescriptor *pSrc, NvRect SrcRect)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvError Err;
    NvDdk2dBlitParameters TexParam = {0};
    NvDdk2dSurface *pSrcDdk2dSurface = NULL;
    NvDdk2dSurface *pDstDdk2dSurface = NULL;
    NvDdk2dFixedRect SrcRectLocal;
    NvDdk2dHandle h2d = NvxGet2d();
    NvDdk2dSurfaceType Type;

    if (!h2d)
        return OMX_ErrorBadParameter;

    if(pSrc->SurfaceCount == 1) {
        Type = NvDdk2dSurfaceType_Single;
    } else if(pSrc->SurfaceCount == 2) {
        Type = NvDdk2dSurfaceType_Y_UV;
    } else {
        Type = NvDdk2dSurfaceType_Y_U_V;
    }
    NvxLock2d();

    // Setup surfaces
    Err = NvDdk2dSurfaceCreate(h2d, Type,
                               &(pSrc->Surfaces[0]), &pSrcDdk2dSurface);
    if (NvSuccess != Err)
    {
        eError = OMX_ErrorUndefined;
        goto L_cleanup;
    }
    Err = NvDdk2dSurfaceCreate(h2d, NvDdk2dSurfaceType_Y_U_V,
                               &(pDest->Surfaces[0]), &pDstDdk2dSurface);
    if (NvSuccess != Err)
    {
        eError = OMX_ErrorUndefined;
        goto L_cleanup;
    }

    // Set attributes
    NvDdk2dSetBlitFilter(&TexParam, NvDdk2dStretchFilter_Nicest);

    NvDdk2dSetBlitTransform(&TexParam, NvDdk2dTransform_None);

    ConvertRect2Fx(&SrcRect, &SrcRectLocal);

    Err = NvDdk2dBlit(h2d, pDstDdk2dSurface, &DstRect,
                      pSrcDdk2dSurface, &SrcRectLocal, &TexParam);
    if (NvSuccess != Err)
    {
        eError = OMX_ErrorUndefined;
        goto L_cleanup;
    }

    Nvx2dFlush(pDstDdk2dSurface);

L_cleanup:
    if (pSrcDdk2dSurface)
        NvDdk2dSurfaceDestroy(pSrcDdk2dSurface);
    if (pDstDdk2dSurface)
        NvDdk2dSurfaceDestroy(pDstDdk2dSurface);

    NvxUnlock2d();

    return eError;
}

// Bad rects can cause system crash.
static void
rectErrCheck(NvRect *sRect)
{
    if (sRect->left > sRect->right) {
        NV_DEBUG_PRINTF(("Correcting bad L/R %d->%d", sRect->left, sRect->right));
        sRect->right = sRect->left;
    }

    if (sRect->top > sRect->bottom) {
        NV_DEBUG_PRINTF(("Correcting bad T/B %d->%d", sRect->top, sRect->bottom));
        sRect->bottom = sRect->top;
    }
}

static void
aspectratio_crop(
    NvU32 inW,
    NvU32 inH,
    float inPAR,
    NvU32 outW,
    NvU32 outH,
    float outPAR,
    NvU32 *pCalcW,
    NvU32 *pCalcH)
{
    float DAR = inPAR * inW / inH;
    float ODAR = outPAR * outW / outH;

    NV_ASSERT(pCalcW && pCalcH);

    *pCalcW = inW;
    *pCalcH = inH;

    if (DAR < ODAR)
        *pCalcH = (int)(0.5 + inW * inPAR / ODAR);
    else if (DAR > ODAR)
        *pCalcW = (int)(0.5 + inH * ODAR / inPAR);

    NV_DEBUG_PRINTF(("%s in %dx%d out:%dx%d calcinput %dx%d",
        __FUNCTION__,
        inW,
        inH,
        outW,
        outH,
        *pCalcW,
        *pCalcH));
}

static void
aspectratio_fit(
    NvU32 inW,
    NvU32 inH,
    float inPAR,
    NvU32 outW,
    NvU32 outH,
    float outPAR,
    NvU32 *pCalcW,
    NvU32 *pCalcH)
{
    float DAR = inPAR * inW / inH;
    float ODAR = outPAR * outW / outH;

    NV_ASSERT(pCalcW && pCalcH);

    *pCalcW = outW;
    *pCalcH = outH;
    if (DAR < ODAR)
        *pCalcW = (int)(0.5 + outH * DAR);
    else if (DAR > ODAR)
        *pCalcH = (int)(0.5 + outW / DAR);

    NV_DEBUG_PRINTF(("%s in %dx%d out:%dx%d calcoutput %dx%d",
        __FUNCTION__,
        inW,
        inH,
        outW,
        outH,
        *pCalcW,
        *pCalcH));
}

static void
Handle2DProcessing(
    NVX_CONFIG_VIDEO2DPROCESSING *pVPP,
    NvMMSurfaceDescriptor *pSrc,
    NvRect *pSrcRect,
    NvMMSurfaceDescriptor *pDst,
    NvRect *pDstRect,
    NvDdk2dTransform *pTrans,
    NvRect *pBC1)
{
    NvU32 inpW, outW, inpH, outH;
    NvU32 rot = 0;
    NvU32 calcW, calcH;
    NvU32 right, bottom;
    NvU32 Vedge=0, Hedge=0;
    NvBool bRotate = NV_FALSE;

    if (pVPP->nSetupFlags & NVX_V2DPROC_FLAG_SOURCERECTANGLE)
    {
        right = pVPP->nSrcLeft + pVPP->nSrcWidth - 1;
        bottom = pVPP->nSrcTop + pVPP->nSrcHeight - 1;

        if (right <= pSrc->Surfaces[0].Width && bottom <= pSrc->Surfaces[0].Height)
        {
            pSrcRect->left = pVPP->nSrcLeft;
            pSrcRect->top = pVPP->nSrcTop;
            pSrcRect->right = right;
            pSrcRect->bottom = bottom;
        }
        else
        {
            NV_DEBUG_PRINTF(("%s Source Rect outside of surface. (%d,%d) Use Orig SrcRect",
                __FUNCTION__,
                right,
                bottom));
        }
    }

    if (pVPP->nSetupFlags & NVX_V2DPROC_FLAG_DESTINATIONRECTANGLE)
    {
        right = pVPP->nDstLeft + pVPP->nDstWidth - 1;
        bottom = pVPP->nDstTop + pVPP->nDstHeight - 1;
        if (right <= pDst->Surfaces[0].Width && bottom <= pDst->Surfaces[0].Height)
        {
            pDstRect->left = pVPP->nDstLeft;
            pDstRect->top = pVPP->nDstTop;
            pDstRect->right = right;
            pDstRect->bottom = bottom;
        }
        else
        {
            NV_DEBUG_PRINTF(("%s Dest Rect outside of surface (%d,%d) Use Orig DstRect",
                __FUNCTION__,
                right,
                bottom));
        }
    }

    // FIXME NvRect definition "excludes" bottom and right row so there should be a +1 below.
    // But this convention is not being followed in nvmmlite. Not sure if that will cause boundary issues.
    inpW = pSrcRect->right - pSrcRect->left + 1;
    inpH = pSrcRect->bottom - pSrcRect->top + 1;
    outW = pDstRect->right - pDstRect->left + 1;
    outH = pDstRect->bottom - pDstRect->top + 1;

    if (pVPP->nSetupFlags & NVX_V2DPROC_FLAG_ROTATION)
    {
        rot = pVPP->nRotation;

        // DDK2DBlit's definition of rotation is counter-clockwise, while OMX
        // requires clockwise rotation. (as per IL spec, though AL spec does
        // not mention it explicitly)
        if (rot == 90)
            rot = 270;
        else if (rot == 270)
            rot = 90;
    }

    if (rot == 90 || rot == 270)
    {
        EXCHANGE(inpW, inpH);
        bRotate = NV_TRUE;
    }

    if (!(pVPP->nSetupFlags & NVX_V2DPROC_FLAG_SCALEOPTIONS))
    {
        pVPP->nScaleOption = NVX_V2DPROC_VIDEOSCALE_FIT;
    }

    switch (pVPP->nScaleOption)
    {
        case NVX_V2DPROC_VIDEOSCALE_STRETCH:
            NV_DEBUG_PRINTF(("%s Scale Stretch", __FUNCTION__));
            break;

        case NVX_V2DPROC_VIDEOSCALE_CROP:
            aspectratio_crop(inpW, inpH, 1.0, outW, outH, 1.0, &calcW, &calcH);
            Vedge = (inpW - calcW) / 2;
            Hedge = (inpH - calcH) / 2;

            if (bRotate == NV_TRUE)
            {
                EXCHANGE(Vedge, Hedge);
            }

            pSrcRect->left += Vedge;
            pSrcRect->right -= Vedge; pSrcRect->top += Hedge;
            pSrcRect->bottom -= Hedge;
            NV_DEBUG_PRINTF(("%s calcW:%d calcH:%d Vedge:%d Hedge:%d SrcRect %d,%d-%d,%d",
                __FUNCTION__,
                calcW,
                calcH,
                Vedge,
                Hedge,
                pSrcRect->left,
                pSrcRect->top,
                pSrcRect->right,
                pSrcRect->bottom));
            rectErrCheck(pSrcRect);
            break;

        case NVX_V2DPROC_VIDEOSCALE_FIT:
            aspectratio_fit(inpW, inpH, 1.0, outW, outH, 1.0, &calcW, &calcH);
            Vedge = (outW - calcW) / 2;
            Hedge = (outH - calcH) / 2;

            NvOsMemcpy(pBC1, pDstRect, sizeof(NvRect));

            pDstRect->left += Vedge;
            pDstRect->right -= Vedge;
            pDstRect->top += Hedge;
            pDstRect->bottom -= Hedge;
            NV_DEBUG_PRINTF(("%s calcW:%d calcH:%d Vedge:%d Hedge:%d DstRect %d,%d-%d,%d",
                __FUNCTION__,
                calcW,
                calcH,
                Vedge,
                Hedge,
                pDstRect->left,
                pDstRect->top,
                pDstRect->right,
                pDstRect->bottom));
            rectErrCheck(pDstRect);
            break;

        default:
            NV_DEBUG_PRINTF(("%s Bad Video scale value:%x", __FUNCTION__, pVPP->nScaleOption));
            break;
    }

    *pTrans = ConvertRotationMirror2Transform(rot, pVPP->eMirror);
    NV_DEBUG_PRINTF(("%s Rot:%x eMirror:%x trans:%x",
        __FUNCTION__,
        pVPP->nRotation,
        pVPP->eMirror,
        *pTrans));

}

OMX_ERRORTYPE NvxCopyMMSurfaceDescriptor2DProc(NvMMSurfaceDescriptor *pDst, NvRect DstRect,
    NvMMSurfaceDescriptor *pSrc, NvRect SrcRect, NVX_CONFIG_VIDEO2DPROCESSING *pVPP,
    NvBool bApplyBackground)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvError Err;
    NvDdk2dBlitParameters TexParam = {0};
    NvDdk2dSurface *pSrcDdk2dSurface = NULL;
    NvDdk2dSurface *pDstDdk2dSurface = NULL;
    NvDdk2dFixedRect SrcRectLocal;
    NvDdk2dTransform trans = NvDdk2dTransform_None;
    NvDdk2dHandle h2d = NvxGet2d();
    NvRect bcRect1;

    NvOsMemset(&bcRect1, 0, sizeof(bcRect1));

    if (!h2d)
        return OMX_ErrorBadParameter;

    if (pVPP->nSetupFlags)
    {
        NV_DEBUG_PRINTF(("%s Before VPP 2D %d,%d,%d,%d -> %d,%d,%d,%d applybgcolor:%d\n",
            __FUNCTION__,
            SrcRect.left,
            SrcRect.top,
            SrcRect.right,
            SrcRect.bottom,
            DstRect.left,
            DstRect.top,
            DstRect.right,
            DstRect.bottom,
            bApplyBackground));

        Handle2DProcessing(pVPP, pSrc, &SrcRect, pDst, &DstRect, &trans, &bcRect1);

        NV_DEBUG_PRINTF(("%s After %d,%d,%d,%d -> %d,%d,%d,%d\n",
            __FUNCTION__,
            SrcRect.left,
            SrcRect.top,
            SrcRect.right,
            SrcRect.bottom,
            DstRect.left,
            DstRect.top,
            DstRect.right,
            DstRect.bottom));
    }

    NvxLock2d();

    // Setup surfaces
    Err = NvDdk2dSurfaceCreate(
            h2d,
            NvDdk2dSurfaceType_Y_U_V,
            &(pSrc->Surfaces[0]),
            &pSrcDdk2dSurface);
    if (NvSuccess != Err)
    {
        eError = OMX_ErrorUndefined;
        goto L_cleanup;
    }
    Err = NvDdk2dSurfaceCreate(
            h2d,
            NvDdk2dSurfaceType_Y_U_V,
            &(pDst->Surfaces[0]),
            &pDstDdk2dSurface);
    if (NvSuccess != Err)
    {
        eError = OMX_ErrorUndefined;
        goto L_cleanup;
    }

    if (bApplyBackground)
    {
        // Now apply background color. Ideally we can work with two rects,
        // but the color is applied only once, so we apply it over the
        // full destination rectangle.
        NvDdk2dBlitParameters bp;
        NvDdk2dColor fill;
        bp.ValidFields = 0;

        // FIXME: If we switch to a VPP test with a smaller destination
        // rectangle, then there will be leftovers of the previous capture
        // in the output. We could avoid this by filling black in the
        // whole capture surface first. Comment out if not needed.
        fill.Format = NvColorFormat_A8B8G8R8;
        *((NvU32 *)fill.Value) = 0;
        NvDdk2dSetBlitFill(&bp, &fill);
        Err = NvDdk2dBlit(h2d, pDstDdk2dSurface, NULL, NULL, NULL, &bp);

        if ((bcRect1.right != 0) && (bcRect1.bottom != 0))
        {
            fill.Format = NvColorFormat_A8B8G8R8;
            *((NvU32 *)fill.Value) = pVPP->nBackgroundColor;
            NvDdk2dSetBlitFill(&bp, &fill);

            NV_DEBUG_PRINTF(("%s Applying background color %x to %d,%d-%d,%d",
                __FUNCTION__,
                pVPP->nBackgroundColor,
                bcRect1.left,
                bcRect1.top,
                bcRect1.right,
                bcRect1.bottom));
            Err = NvDdk2dBlit(h2d, pDstDdk2dSurface, &bcRect1, NULL, NULL, &bp);
        }
    }

    // Set attributes
    NvDdk2dSetBlitFilter(&TexParam, NvDdk2dStretchFilter_Nicest);

    NvDdk2dSetBlitTransform(&TexParam, trans);

    ConvertRect2Fx(&SrcRect, &SrcRectLocal);

    Err = NvDdk2dBlit(
            h2d,
            pDstDdk2dSurface,
            &DstRect,
            pSrcDdk2dSurface,
            &SrcRectLocal,
            &TexParam);
    if (NvSuccess != Err)
    {
        eError = OMX_ErrorUndefined;
        goto L_cleanup;
    }

    Nvx2dFlush(pDstDdk2dSurface);


L_cleanup:
    if (pSrcDdk2dSurface)
        NvDdk2dSurfaceDestroy(pSrcDdk2dSurface);
    if (pDstDdk2dSurface)
        NvDdk2dSurfaceDestroy(pDstDdk2dSurface);

    NvxUnlock2d();

    return eError;
}

OMX_ERRORTYPE NvxTranslateErrorCode(NvError eError)
{
    switch(eError)
    {
        case NvSuccess:                          return OMX_ErrorNone;
        case NvError_NotImplemented:             return OMX_ErrorNotImplemented;
        case NvError_NotSupported:               return OMX_ErrorUnsupportedSetting;
        case NvError_NotInitialized:             return OMX_ErrorInsufficientResources;
        case NvError_BadParameter:               return OMX_ErrorBadParameter;
        case NvError_InsufficientMemory:         return OMX_ErrorInsufficientResources;
        case NvError_Timeout:                    return OMX_ErrorTimeout;
        case NvError_InvalidState:               return OMX_ErrorInvalidState;
        case NvError_BadValue:                   return OMX_ErrorBadParameter;
        case NvError_Busy:                       return OMX_ErrorNotReady;
        case NvError_ModuleNotPresent:           return OMX_ErrorComponentNotFound;
        case NvError_ResourceError:              return OMX_ErrorInsufficientResources;
        case NvError_InsufficientVideoMemory:    return OMX_ErrorInsufficientResources;
        case NvError_VideoDecUnsupportedStreamFormat: return OMX_ErrorFormatNotDetected;
        case NvError_ParserDRMLicenseNotFound:   return NvxError_ParserDRMLicenseNotFound;
        case NvError_ParserDRMFailure:           return NvxError_ParserDRMFailure;
        case NvError_ParserCorruptedStream:      return NvxError_ParserCorruptedStream;
        case NvError_ParserSeekUnSupported:      return NvxError_ParserSeekUnSupported;
        case NvError_ParserTrickModeUnSupported: return NvxError_ParserTrickModeUnSupported;
        case NvError_WriterInsufficientMemory : return NvxError_WriterInsufficientMemory;
        case NvError_FileWriteFailed:           return NvxError_FileWriteFailed;
        case NvError_WriterFailure:             return NvxError_WriterFailure;
        case NvError_WriterUnsupportedStream:   return NvxError_WriterUnsupportedStream;
        case NvError_WriterUnsupportedUserData: return NvxError_WriterUnsupportedUserData;
        case NvError_WriterFileSizeLimitExceeded: return NvxError_WriterFileSizeLimitExceeded;
        case NvError_WriterFileWriteLimitExceeded: return NvxError_WriterFileSizeLimitExceeded;
        case NvError_WriterTimeLimitExceeded:     return NvxError_WriterTimeLimitExceeded;
        case NvError_DrmFailure: return NVX_EventDRM_DrmFailure;
        case NvError_CameraHwNotResponding:      return NvxError_CameraHwNotResponding;

        default: return OMX_ErrorUndefined;
    }
}

OMX_U32 NvxGetOsRotation(void)
{
    return 0;
}

void NvxSmartDimmerEnable(NvxOverlay *pOverlay, NvBool enable)
{
    if (pOverlay->bSmartDimmerOn == enable)
        return;

    pOverlay->bSmartDimmerOn = enable;
}

OMX_ERRORTYPE NvxStereoRenderingEnable(NvxOverlay *pOverlay)
{
    return OMX_ErrorNotImplemented;
}

/* Extract NvMM stereo flags from NvMM buffer flags */
NvU32 ExtractStereoFlags(NvU32 bufferFlags)
{
    NvU32 stereoFlags = bufferFlags &
        ( NvMMBufferFlag_StereoEnable |
          NvMMBufferFlag_Stereo_SEI_FPType_Mask |
          NvMMBufferFlag_Stereo_SEI_ContentType_Mask );
    return stereoFlags;
}

/* Convert stereo flags from NvMM buffer flags to NvRM stereo type */
NvStereoType NvMmStereoFlagsToNvRmStereoType(NvU32 stereoFlags)
{
    NvStereoType stereoType;
    // Since these values are defined in nvrm_surface.h as the same,
    // this is not really needed, but it decouples two enumerations
    switch (stereoFlags)
    {
    case NvMMBufferFlag_Stereo_None: stereoType = NV_STEREO_NONE; break;
    case NvMMBufferFlag_Stereo_LeftRight: stereoType = NV_STEREO_LEFTRIGHT; break;
    case NvMMBufferFlag_Stereo_RightLeft: stereoType = NV_STEREO_RIGHTLEFT; break;
    case NvMMBufferFlag_Stereo_TopBottom: stereoType = NV_STEREO_TOPBOTTOM; break;
    case NvMMBufferFlag_Stereo_BottomTop: stereoType = NV_STEREO_BOTTOMTOP; break;
    case NvMMBufferFlag_Stereo_SeparateLR: stereoType = NV_STEREO_SEPARATELR; break;
    case NvMMBufferFlag_Stereo_SeparateRL: stereoType = NV_STEREO_SEPARATERL; break;
    default: stereoType = NV_STEREO_NONE; break;
    }
    return stereoType;
}

/* Convert NvRM stereo type to NvMM buffer flags */
NvU32 NvRmStereoTypeToNvMmStereoFlags(NvStereoType stereoType)
{
    // Since these values are defined in nvrm_surface.h as the same,
    // this is not really needed, but it decouples two enumerations
    NvU32 stereoFlags;
    // Since these values are defined in nvrm_surface.h as the same,
    // this is not really needed, but it decouples two enumerations
    switch (stereoType)
    {
    case NV_STEREO_NONE: stereoFlags = NvMMBufferFlag_Stereo_None; break;
    case NV_STEREO_LEFTRIGHT: stereoFlags = NvMMBufferFlag_Stereo_LeftRight; break;
    case NV_STEREO_RIGHTLEFT: stereoFlags = NvMMBufferFlag_Stereo_RightLeft; break;
    case NV_STEREO_TOPBOTTOM: stereoFlags = NvMMBufferFlag_Stereo_TopBottom; break;
    case NV_STEREO_BOTTOMTOP: stereoFlags = NvMMBufferFlag_Stereo_BottomTop; break;
    case NV_STEREO_SEPARATELR: stereoFlags = NvMMBufferFlag_Stereo_SeparateLR; break;
    case NV_STEREO_SEPARATERL: stereoFlags = NvMMBufferFlag_Stereo_SeparateRL; break;
    default: stereoFlags = NvMMBufferFlag_Stereo_None; break;
    }
    return stereoFlags;
}

/* Extract NvRM stereo type from NvMM buffer flags */
NvStereoType NvStereoTypeFromBufferFlags(NvU32 bufferFlags)
{
    NvU32 stereoFlags = ExtractStereoFlags(bufferFlags);
    NvStereoType stereoType = NvMmStereoFlagsToNvRmStereoType(stereoFlags);
    return stereoType;
}

