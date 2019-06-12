/* Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property 
 * and proprietary rights in and to this software, related documentation 
 * and any modifications thereto.  Any use, reproduction, disclosure or 
 * distribution of this software and related documentation without an 
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef NVXJITTER_H_
#define NVXJITTER_H_

typedef struct NvxJitterTool
{
    OMX_U8 *pName;
    OMX_TICKS *pTicks;
    OMX_U32 nTicksMax;
    OMX_U32 nTickCount;

    OMX_TICKS nLastTime;

    OMX_BOOL bShow;

#define MAX_JITTER_HISTORY 3000
    double fAvg[MAX_JITTER_HISTORY];
    double fStdDev[MAX_JITTER_HISTORY];
    OMX_U32 nPos;
} NvxJitterTool;

NvxJitterTool *NvxAllocJitterTool(const char *pName, OMX_U32 nTicks);
void NvxFreeJitterTool(NvxJitterTool *pTool);

void NvxJitterToolAddPoint(NvxJitterTool *pTool);
void NvxJitterToolSetShow(NvxJitterTool *pTool, OMX_BOOL bShow);

void NvxJitterToolGetAvgs(NvxJitterTool *pTool, double *pStdDev, double *pAvg,
                          double *pHighest);

#endif

