/*
 * Copyright (c) 2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/** nvxlitevppcpu.c */

/* =========================================================================
 *                     I N C L U D E S
 * ========================================================================= */

#include <OMX_Core.h>
#include <nvassert.h>
#include <nvos.h>
#include <NvxTrace.h>
#include <NvxComponent.h>
#include <NvxPort.h>
#include <NvxCompReg.h>
#include <nvmmlitetransformbase.h>
#include <nvxlitevideopostprocess.h>
#include "nvxlitevpp.h"

#include "NvxHelpers.h"
#include "nvxliteandroidbuffer.h"

#if defined(__ARM_NEON__)
#include <arm_neon.h>
#endif // __ARM_NEON__

//#define APPLY_CPU_BUFFER
//#define NO_EFFECT
//#define PROCESS_YPLANE_ONLY


#ifdef APPLY_CPU_BUFFER
static NvError NvVPP_CPU_CopyVideoSurface(SNvxNvMMLiteTransformData *pData, NvMMBuffer* nvbuf, NvMMBuffer *nvout)
{
    NvError status = NvSuccess;
    SNvxNvMMLiteVPP *pVpp = (SNvxNvMMLiteVPP *)&(pData->sVpp);
    NvU32 size;
    NvS32 index;
    NvMMBuffer* buf = nvbuf;
    NvxSurface *pInSurf;
    NvRmSurface *pRmInSurf;
    NvxSurface *pOutSurf;
    NvRmSurface *pRmOutSurf;
    void *pSurfData;
    NvU8 *pCpuBuffer;
    NvU32 offset = 0;

      pInSurf = (NvxSurface *)buf->Payload.Surfaces.Surfaces;
      pRmInSurf = &(pInSurf->Surfaces[0]);
      pVpp->pVideoSurface[0]->width =  pRmInSurf->Width;
      pVpp->pVideoSurface[0]->height =  pRmInSurf->Height;
      pVpp->pVideoSurface[0]->nSurface =  pInSurf->SurfaceCount;

      pOutSurf = (NvxSurface *)nvout->Payload.Surfaces.Surfaces;
      pRmOutSurf = &(pOutSurf->Surfaces[0]);
      pVpp->pVideoSurface[1]->width =  pRmOutSurf->Width;
      pVpp->pVideoSurface[1]->height =  pRmOutSurf->Height;
      pVpp->pVideoSurface[1]->nSurface =  pOutSurf->SurfaceCount;

       size = NvRmSurfaceComputeSize(&nvbuf->Payload.Surfaces.Surfaces[0]);
       //NvOsDebugPrintf(" Calculated Y size is %d", size);
       size += NvRmSurfaceComputeSize(&nvbuf->Payload.Surfaces.Surfaces[1]);
       //NvOsDebugPrintf(" Total size is %d", size);

       pCpuBuffer = (NvU8*)NvOsAlloc(size);
       for (index = 0; index < pInSurf->SurfaceCount; index++)
       {
          NvRmSurfaceRead(
                  &(pInSurf->Surfaces[index]),
                  0,
                  0,
                  pInSurf->Surfaces[index].Width,
                  pInSurf->Surfaces[index].Height,
                  pCpuBuffer);


          NvRmSurfaceWrite(
                  &(pOutSurf->Surfaces[index]),
                  0,
                  0,
                  pOutSurf->Surfaces[index].Width,
                  pOutSurf->Surfaces[index].Height,
                  pCpuBuffer);
      }

     NvOsFree(pCpuBuffer);
     return status;
}
#endif


static NvError NvVPP_CPU_GetVideoSurface(SNvxNvMMLiteTransformData *pData, NvMMBuffer* nvbuf,int inout)
{
    NvError status = NvSuccess;
    NvU32 streamIndex = 1; //for output
    SNvxNvMMLiteVPP *pVpp = (SNvxNvMMLiteVPP *)&(pData->sVpp);
    NvU32 size;
    NvS32 index;
    NvMMBuffer* buf = nvbuf;
    NvxSurface *pSurf;
    NvRmSurface *pRmSurf;
    void *pSurfData;

    pSurf = (NvxSurface *)buf->Payload.Surfaces.Surfaces;
    pRmSurf = &(pSurf->Surfaces[0]);
    pVpp->pVideoSurface[inout]->width =  pRmSurf->Width;
    pVpp->pVideoSurface[inout]->height =  pRmSurf->Height;
    pVpp->pVideoSurface[inout]->nSurface =  pSurf->SurfaceCount;

    size = NvRmSurfaceComputeSize(&nvbuf->Payload.Surfaces.Surfaces[0]);

    status = NvRmMemMap(pRmSurf->hMem,
                        pRmSurf->Offset,
                        size,
                        NVOS_MEM_READ_WRITE,
                        &pSurfData);
    pVpp->pVideoSurface[inout]->pdata[0] =  pSurfData;
#ifndef PROCESS_YPLANE_ONLY
    size = NvRmSurfaceComputeSize(&nvbuf->Payload.Surfaces.Surfaces[1]);
    pRmSurf = &(pSurf->Surfaces[1]);
    status |= NvRmMemMap(pRmSurf->hMem,
                        pRmSurf->Offset,
                        size,
                        NVOS_MEM_READ_WRITE,
                        &pSurfData);
    pVpp->pVideoSurface[inout]->pdata[1] =  pSurfData;
#endif
    return status;
}

static void FreeVPPVideoSurface(SNvxNvMMLiteTransformData *pData, NvMMBuffer* nvbuf,int inout)
{
    NvError status = NvSuccess;
    NvU32 streamIndex = 1; //for output
    SNvxNvMMLiteVPP *pVpp = (SNvxNvMMLiteVPP *)&(pData->sVpp);
    NvU32 size;
    NvMMBuffer* buf = nvbuf;
    NvxSurface *pSurf;
    NvRmSurface *pRmSurf;
    void *pSurfData;

    pSurf = (NvxSurface *)buf->Payload.Surfaces.Surfaces;
    pRmSurf = &(pSurf->Surfaces[0]);
    size = NvRmSurfaceComputeSize(&nvbuf->Payload.Surfaces.Surfaces[0]);
    pSurfData = pVpp->pVideoSurface[inout]->pdata[0];
    NvRmMemUnmap(pRmSurf->hMem, pSurfData,size);

#ifndef PROCESS_YPLANE_ONLY
    size = NvRmSurfaceComputeSize(&nvbuf->Payload.Surfaces.Surfaces[1]);
    pRmSurf = &(pSurf->Surfaces[1]);
    pSurfData = pVpp->pVideoSurface[inout]->pdata[1];
    NvRmMemUnmap(pRmSurf->hMem, pSurfData,size);
#endif
}

static NvError ApplyEffect(VPPSurface *inSurf, VPPSurface *outSurf, NvMMBuffer* pDecSurf, NvMMBuffer* pOutSurf)
{
    NvError status = NvSuccess;
    NvU32 size=0;
    NvU32 width;
    NvU32 height;
    char * pSrcData;
    char * pDstData;
    NvU32 i=0;
    uint8x8_t neg = vdup_n_u8 (255);

    width = NV_MIN(outSurf->width, inSurf->width);
    height = NV_MIN(outSurf->height, inSurf->height);

    size = NvRmSurfaceComputeSize(&pDecSurf->Payload.Surfaces.Surfaces[0]);
    pSrcData = (char *)inSurf->pdata[0];
    pDstData = (char *)outSurf->pdata[0];
#ifdef NO_EFFECT
        NvOsMemcpy(pDstData, pSrcData,size);
#else

    //Negative effect
    for(i=0;i<size;i+=8)
    {
        uint8x8_t ydata  = vld1_u8 ((const uint8_t *)&pSrcData[i]);
        uint8x8_t result;
        result = vsub_u8(neg,ydata);
        vst1_u8 ((uint8_t *)&pDstData[i], result);
    }
#endif

#ifndef PROCESS_YPLANE_ONLY
    size = NvRmSurfaceComputeSize(&pDecSurf->Payload.Surfaces.Surfaces[1]);
    pSrcData = (char *)inSurf->pdata[1];
    pDstData = (char *)outSurf->pdata[1];
    NvOsMemcpy(pDstData, pSrcData,size);
#endif
    return status;
}

NvError NvVPPApplyCPUBasedEffect(SNvxNvMMLiteTransformData* pData, NvMMBuffer* pDecSurf, NvMMBuffer* pOutSuface, void* cookie)
{
    NvError status = NvSuccess;

#ifndef APPLY_CPU_BUFFER
    status = NvVPP_CPU_GetVideoSurface(pData,pDecSurf,VPP_SURFACE_IN);
    if( status != NvSuccess)
    {
       NvOsDebugPrintf("NvRmMemMap IN Fail");
       return status;
    }

    status = NvVPP_CPU_GetVideoSurface(pData,pOutSuface,VPP_SURFACE_OUT);
    if( status != NvSuccess)
    {
       NvOsDebugPrintf("NvRmMemMap OUT Fail");
       FreeVPPVideoSurface(pData,pDecSurf,VPP_SURFACE_IN);
       return status;
    }
    pOutSuface->Payload.Surfaces.CropRect = pDecSurf->Payload.Surfaces.CropRect;
#else
    status = NvVPP_CPU_CopyVideoSurface(pData, pDecSurf, pOutSuface);
    return status;
#endif
    status = ApplyEffect(pData->sVpp.pVideoSurface[VPP_SURFACE_IN],pData->sVpp.pVideoSurface[VPP_SURFACE_OUT], pDecSurf, pOutSuface);

#ifndef APPLY_CPU_BUFFER
    FreeVPPVideoSurface(pData,pDecSurf,VPP_SURFACE_IN);
    FreeVPPVideoSurface(pData,pOutSuface,VPP_SURFACE_OUT);
#endif
    return status;
}
