/* Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __NVX_VIDEO_POSTPROCESSING_H
#define __NVX_VIDEO_POSTPROCESSING_H


#define MAX_NVMM_BUFFERS        32
#define NUM_VPP_SURFACES        2
#define TOTAL_NUM_SURFACES      MAX_NVMM_BUFFERS
#define FRAME_FREE              101
#define FRAME_SENT_FOR_DISPLAY  102
#define VPP_SURFACE_IN          0
#define VPP_SURFACE_OUT         1

typedef struct {
   NvU32 width;
   NvU32 height;
   NvU32 pitch;     //aka stride
   NvU32 nSurface;
   void  *pdata[6];
} VPPSurface;

typedef struct SNvxNvMMLiteVPP
{
    OMX_BOOL              bVppThreadActive;
    volatile OMX_BOOL     bVppThreadClose;
    volatile OMX_BOOL     bVppStop;
    volatile OMX_BOOL     bVppThreadtopuase;
    NvOsThreadHandle      hVppThread;
    NvOsSemaphoreHandle   VppInpOutAvail;
    NvOsSemaphoreHandle   VppThreadPaused;
    NvOsSemaphoreHandle   VppInitDone;
    NvMMQueueHandle       pReleaseOutSurfaceQueue;
    NvMMQueueHandle       pOutSurfaceQueue;
    NvU32                 Renderstatus[TOTAL_NUM_SURFACES];
    NvU32                 Rendstatus[TOTAL_NUM_SURFACES];
    NvU32                 VideoSurfaceIdx;
    NvMMBuffer            *pVideoSurf[TOTAL_NUM_SURFACES];
    NvU32                 frameWidth;
    NvU32                 frameHeight;
    NvU64                 prevTimeStamp;
    VPPSurface           *pVideoSurface[2];         //0: Input surface 1: Output Surface
    NvMMBuffer           *pCurrSurface;
    NvU32                 importBufferID;
    OMX_HANDLETYPE        oVPPMutex;
    OMX_COLOR_FORMATTYPE  colorformat;
    OMX_BOOL              bVppEnable;
    NVX_VPP_TYPE          nVppType;
}SNvxNvMMLiteVPP;

NvError NvxVPPcreate(void *arg);
void NvxVPPdestroy(void *arg);
NvError NvxVPPallocVideoSurface(void *arg);
void NvxVPPThread(void* arg);
void NvxVPPFlush(void* arg);
void VPPClose(SNvxNvMMLiteVPP *pVpp);
NvError ReturnUsedNvMMSurface(SNvxNvMMLiteVPP *pVpp, NvMMBuffer* buffer);
NvMMBuffer* GetFreeNvMMSurface(SNvxNvMMLiteVPP *pVpp);


#endif // __NVX_VIDEO_POSTPROCESSING_H
