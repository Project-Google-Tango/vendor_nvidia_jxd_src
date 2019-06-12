/* Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property 
 * and proprietary rights in and to this software, related documentation 
 * and any modifications thereto.  Any use, reproduction, disclosure or 
 * distribution of this software and related documentation without an 
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include <nvos.h>
#include <nvassert.h>
#include <OMX_Core.h>
#include <math.h>

#include "nvxjitter.h"

NvxJitterTool *NvxAllocJitterTool(const char *pName, OMX_U32 nTicks)
{
    NvxJitterTool *pTool;

    NV_ASSERT(pName);
    NV_ASSERT(nTicks > 0);

    pTool = NvOsAlloc(sizeof(NvxJitterTool));
    if (!pTool)
        return NULL;

    NvOsMemset(pTool, 0, sizeof(NvxJitterTool));
    pTool->pName = NvOsAlloc(NvOsStrlen(pName) + 1);
    if (!pTool->pName)
    {
        NvOsFree(pTool);
        return NULL;
    }
    NvOsStrncpy((char *)pTool->pName, pName, NvOsStrlen(pName) + 1);

    pTool->pTicks = NvOsAlloc(sizeof(OMX_TICKS) * nTicks);
    if (!pTool->pTicks)
    {
        NvOsFree(pTool->pName);
        NvOsFree(pTool);
        return NULL;
    }

    pTool->nTicksMax = nTicks;
    pTool->nTickCount = 0;
    pTool->nLastTime = 0;
    pTool->bShow = 0;

    return pTool;
}

void NvxFreeJitterTool(NvxJitterTool *pTool)
{
    if (!pTool)
        return;
    NvOsFree(pTool->pName);
    NvOsFree(pTool->pTicks);
    NvOsFree(pTool);
}

void NvxJitterToolAddPoint(NvxJitterTool *pTool)
{
    OMX_TICKS now;
    NV_ASSERT(pTool);

    now = NvOsGetTimeUS();

    if (pTool->nLastTime == 0)
    {
        pTool->nLastTime = now;
        return;
    }

    pTool->pTicks[pTool->nTickCount] = now - pTool->nLastTime;
    pTool->nLastTime = now;
    pTool->nTickCount++;

    if (pTool->nTickCount < pTool->nTicksMax)
        return;

    {
        double fAvg = 0;
        double fStdDev = 0;
        OMX_U32 i;

        for (i = 0; i < pTool->nTicksMax; i++)
            fAvg += pTool->pTicks[i];
        fAvg /= pTool->nTicksMax;

        for (i = 0; i < pTool->nTicksMax; i++)
            fStdDev += (fAvg - pTool->pTicks[i]) * (fAvg - pTool->pTicks[i]);
        fStdDev = sqrt(fStdDev / (pTool->nTicksMax - 1));

        if (pTool->bShow)
            NvOsDebugPrintf("%s: mean: %.2f  std. dev: %.2f\n", pTool->pName, 
                            fAvg, fStdDev);

        if (pTool->nPos < MAX_JITTER_HISTORY)
        {
            pTool->fAvg[pTool->nPos] = fAvg;
            pTool->fStdDev[pTool->nPos] = fStdDev;
            pTool->nPos++;
        } 
    }

    pTool->nTickCount = 0;
}

void NvxJitterToolSetShow(NvxJitterTool *pTool, OMX_BOOL bShow)
{
    pTool->bShow = bShow;
}

void NvxJitterToolGetAvgs(NvxJitterTool *pTool, double *pStdDev, double *pAvg, 
                          double *pHighest)
{
    OMX_U32 i;

    NV_ASSERT(pTool);
    NV_ASSERT(pStdDev);
    NV_ASSERT(pAvg);
    NV_ASSERT(pHighest);

    *pStdDev = 0;
    *pAvg = 0;
    *pHighest = 0;

    if (pTool->nPos < 1)
        return;

    for (i = 1; i < pTool->nPos && i < MAX_JITTER_HISTORY; i++)
    {
        *pAvg = *pAvg + pTool->fAvg[i];
        *pStdDev = *pStdDev + pTool->fStdDev[i];

        if (pTool->fStdDev[i] > *pHighest)
            *pHighest = pTool->fStdDev[i];
    }

    *pAvg = *pAvg / (pTool->nPos);
    *pStdDev = *pStdDev / (pTool->nPos);
}

