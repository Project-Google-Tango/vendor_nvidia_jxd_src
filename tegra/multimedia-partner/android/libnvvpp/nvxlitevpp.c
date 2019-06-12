/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/** nvxlitevpp.c */

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


NvError VppHALInit(SNvxNvMMLiteTransformData *pData)
{
     NvError status = NvSuccess;
     switch(pData->sVpp.nVppType)
     {
          case NV_VPP_TYPE_EGL:
          {
               NvVPPEGLBasedInit(pData, &customcookie);
               break;
          }
          case NV_VPP_TYPE_CPU:
          case NV_VPP_TYPE_CUDA:
          default:
          {
               NvOsDebugPrintf("Not yet implemented!!!!");
          }
     }
     return status;
}

NvError VppHALDestroy(SNvxNvMMLiteTransformData *pData)
{
     NvError status = NvSuccess;
     switch(pData->sVpp.nVppType)
     {
         case NV_VPP_TYPE_EGL:
         {
               status = NvVPPEGLBasedDestroy(pData, &customcookie);
               break;
         }
         case NV_VPP_TYPE_CPU:
         case NV_VPP_TYPE_CUDA:
         default:
         {
               NvOsDebugPrintf("Not yet implemented!!!!");
         }
   }
   return status;
}

NvError VppHALProcess(SNvxNvMMLiteTransformData *pData, NvMMBuffer* pDecSurf, NvMMBuffer* pOutSuface)
{
     NvError status = NvSuccess;
     NvU32 size;
     NvU32 streamIndex = 1; //for output
     int64_t curTime;
     NvMMBuffer* output = pOutSuface;
     NvxLiteOutputBuffer OutBuf;
     SNvxNvMMLitePort *pPort = &(pData->oPorts[streamIndex]);
     NvxPort *pPortOut = pPort->pOMXPort;

     NV_DEBUG_PRINTF(("VppProcess:OMX Buf [%d],pCurrSurf buf[%d] ",output->BufferID,pData->sVpp.pCurrSurface->BufferID));


     size = sizeof(output->PayloadInfo);
     NvOsMemcpy(&(output->PayloadInfo),&(pData->sVpp.pCurrSurface->PayloadInfo),size);
     curTime = NvOsGetTimeUS();


    switch(pData->sVpp.nVppType) {
       case NV_VPP_TYPE_CPU:
       {
            status = NvVPPApplyCPUBasedEffect(pData, pDecSurf, output, &customcookie);
            break;
       }
       case NV_VPP_TYPE_EGL:
       {
            status = NvVPPApplyEGLBasedEffect(pData, pDecSurf, output, &customcookie);
            break;
       }
       case NV_VPP_TYPE_CUDA:
       default:
       {
            NvOsDebugPrintf("Not yet implemented!!!!");
            return status;
       }
    }

    NV_DEBUG_PRINTF(("Time for VPP Process %lld usec",(NvOsGetTimeUS() - curTime)));
    pData->sVpp.Rendstatus[output->BufferID] = OMX_TRUE;

    OutBuf.Buffer = *((NvMMBuffer *)output);
    OutBuf.streamIndex = streamIndex;
    status = NvMMQueueEnQ(pData->pOutputQ[pPort->nOutputQueueIndex], &OutBuf, 30);
    if (NvSuccess != status)
    {
        NvOsDebugPrintf("ERROR: buffer enqueue failed line %d 0x%x \n", __LINE__, status);
        return status;
    }
    return status;
}
