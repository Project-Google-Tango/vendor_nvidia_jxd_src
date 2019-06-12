/*
 * Copyright (c) 2013-2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#if (BUILD_FOR_AOS == 0)
#include <linux/ioctl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <ad5823.h>

#include "focuser_ad5823.h"
#include "imager_hal.h"
#include "imager_util.h"
#include "nvassert.h"
#include "nvodm_tags.h"
#include "nv_log.h"
#include "camera_trace.h"

#define FOCUSER_DB_PRINTF(...)
//#define FOCUSER_DB_PRINTF(...)  NvOsDebugPrintf(__VA_ARGS__)


#define FOCUSER_POSITIONS(ctx) ((ctx)->Config.pos_high - (ctx)->Config.pos_low)
#define GRAVITY_FUDGE(x) (85 * (x) / 100)

#define TIME_DIFF(_from, _to) \
    (((_from) <= (_to)) ? ((_to) - (_from)) : \
     (~0UL - ((_from) - (_to)) + 1))

/**
 * Focuser's context
 */
typedef struct
{
    int focuser_fd;
    NvOdmImagerPowerLevel PowerLevel;
    NvU32 cmdTime;             /* time of latest focus command issued */
    NvU32 Position;            /* The last settled focus position. */
    NvU32 RequestedPosition;   /* The last requested focus position. */
    NvS32 DelayedFPos;         /* Focusing position for delayed request */
    struct nv_focuser_config Config;
} FocuserCtxt;

static void Focuser_UpdateSettledPosition(FocuserCtxt *ctxt)
{
    NvU32 CurrentTime = 0;

    /*  Settled position has been updated? */
    if (ctxt->Position == ctxt->RequestedPosition)
        return;

    CurrentTime = NvOdmOsGetTimeMS();

    /*  Previous requested position settled? */
    if (TIME_DIFF(ctxt->cmdTime, CurrentTime) >= ctxt->Config.focuser_set[0].settle_time)
        ctxt->Position = ctxt->RequestedPosition;
}

/**
 * setPosition.
 */
static NvBool setPosition(FocuserCtxt *ctxt, NvU32 Position)
{
    /*  no need to update if position has not changed */
    if (Position == ctxt->RequestedPosition)
        return NV_TRUE;

    NV_TRACE_BEGIN_V(NV_ODM_FOCUSER_TAG);
    if (Position < ctxt->Config.pos_actual_low && Position > ctxt->Config.pos_actual_high)
    {
        FOCUSER_DB_PRINTF("AD5823 %s: position %d out of bounds\n", __func__, Position);
        NV_TRACE_END();
        return NV_FALSE;
    }

    FOCUSER_DB_PRINTF("AD5823 %s: position = %d\n", __func__, Position);

    if (ioctl(ctxt->focuser_fd, AD5823_IOCTL_SET_POSITION, (void *)(Position)) < 0)
    {
        NvOsDebugPrintf("AD5823 %s: ioctl to set focus failed - %s\n",
                __FILE__,
                strerror(errno));
        NV_TRACE_END();
        return NV_FALSE;
    }

    ctxt->RequestedPosition = Position;
    ctxt->cmdTime = NvOdmOsGetTimeMS();
    NV_TRACE_END();
    return NV_TRUE;
}

static void FocuserAD5823_Close(NvOdmImagerHandle hImager);

/**
 * Focuser_Open
 * Initialize Focuser's private context.
 */
static NvBool FocuserAD5823_Open(NvOdmImagerHandle hImager)
{
#if (BUILD_FOR_AOS == 0)

    FocuserCtxt *ctxt = NULL;

    FOCUSER_DB_PRINTF("Focuser_Open\n");

    if (!hImager || !hImager->pFocuser)
        return NV_FALSE;

    NV_TRACE_BEGIN_V(NV_ODM_FOCUSER_TAG);
    ctxt = NvOdmOsAlloc(sizeof(*ctxt));
    if (!ctxt)
        goto fail;
    hImager->pFocuser->pPrivateContext = ctxt;

    NvOdmOsMemset(ctxt, 0, sizeof(FocuserCtxt));

    /*
      The file descriptor manipulation is done now in _Open/_Close
      instead of _SetPowerLevel, because _GetParameter queries info
      available only from the sensor driver (i.e. not hardcoded here).
     */

#ifdef O_CLOEXEC
    ctxt->focuser_fd = open("/dev/focuser", O_RDWR|O_CLOEXEC);
#else
    ctxt->focuser_fd = open("/dev/focuser", O_RDWR);
#endif

    if (ctxt->focuser_fd < 0)
    {
        NvOsDebugPrintf("Can not open focuser device: %s\n", strerror(errno));
        NV_TRACE_END();
        return NV_FALSE;
    }

    if (ioctl(ctxt->focuser_fd, AD5823_IOCTL_GET_CONFIG, &ctxt->Config) < 0)
    {
        NvOsDebugPrintf("Can not open get focuser config: %s\n", strerror(errno));
        close(ctxt->focuser_fd);
        ctxt->focuser_fd = -1;
        NV_TRACE_END();
        return NV_FALSE;
    }
    FOCUSER_DB_PRINTF("ad5823 config: range[%d, %d], settle_time: %d\n",
                      ctxt->Config.pos_working_low, ctxt->Config.pos_working_high,
                      ctxt->Config.focuser_set[0].settle_time);


    ctxt->PowerLevel = NvOdmImagerPowerLevel_Off;
    ctxt->cmdTime = 0;
    ctxt->RequestedPosition = ctxt->Position = 0;
    ctxt->DelayedFPos = -1;

    NV_TRACE_END();
    return NV_TRUE;

fail:
    NvOsDebugPrintf("Focuser_Open FAILED\n");
    FocuserAD5823_Close(hImager);
    NV_TRACE_END();
    return NV_FALSE;
#else
	return NV_TRUE;
#endif
}

/**
 * Focuser_Close
 * Free focuser's context and resources.
 */
static void FocuserAD5823_Close(NvOdmImagerHandle hImager)
{
    FocuserCtxt *pContext = NULL;

    if (hImager && hImager->pFocuser && hImager->pFocuser->pPrivateContext)
    {
        pContext = (FocuserCtxt *) hImager->pFocuser->pPrivateContext;
#if (BUILD_FOR_AOS == 0)
        close(pContext->focuser_fd);
#endif
        NvOdmOsFree(pContext);
        hImager->pFocuser->pPrivateContext = NULL;
    }
}

/**
 * Focuser_SetPowerLevel
 * Set the focuser's power level.
 */
static NvBool FocuserAD5823_SetPowerLevel(NvOdmImagerHandle hImager,
                NvOdmImagerPowerLevel PowerLevel)
{
    FocuserCtxt *pContext =
        (FocuserCtxt *) hImager->pFocuser->pPrivateContext;
    NvBool Status = NV_TRUE;
    NvS32 dPos;

    NV_TRACE_BEGIN_V(NV_ODM_FOCUSER_TAG);
    FOCUSER_DB_PRINTF("Focuser_SetPowerLevel (%d)\n", PowerLevel);

    if (pContext->PowerLevel == PowerLevel)
    {
        NV_TRACE_END();
        return NV_TRUE;
    }

    switch(PowerLevel)
    {
        case NvOdmImagerPowerLevel_On:

            /* Check for delayed focusing request: */
            dPos = pContext->DelayedFPos;
            if (dPos >= 0)
            {
                Status = setPosition(pContext, dPos);
                pContext->DelayedFPos = -1;
            }
            break;
        case NvOdmImagerPowerLevel_Off:
            break;
        default:
            FOCUSER_DB_PRINTF("Focuser taking power level %d\n", PowerLevel);
            break;
    }

    pContext->PowerLevel = PowerLevel;
    NV_TRACE_END();
    return Status;
}

/**
 * Focuser_GetCapabilities
 * Get focuser's capabilities.
 */
static void FocuserAD5823_GetCapabilities(NvOdmImagerHandle hImager,
            NvOdmImagerCapabilities *pCapabilities)
{
    FOCUSER_DB_PRINTF("Focuser_GetCapabilities - stubbed\n");
}

/**
 * Focuser_SetParameter
 * Set focuser's parameter
 */
static NvBool FocuserAD5823_SetParameter(NvOdmImagerHandle hImager,
            NvOdmImagerParameter Param,
            NvS32 SizeOfValue, const void *pValue)
{
    NvBool Status = NV_FALSE;
    FocuserCtxt *pContext = (FocuserCtxt *) hImager->pFocuser->pPrivateContext;
    NvU32 Position;
    NvU32 i, j;

    FOCUSER_DB_PRINTF("Focuser_SetParameter (%d)\n", Param);

    switch (Param)
    {
        case NvOdmImagerParameter_FocuserLocus:
        {
            Position = *((NvU32 *) pValue);
            FOCUSER_DB_PRINTF("NvOdmImagerParameter_FocuserLocus: Position=%d\n", Position);

            if (pContext->PowerLevel != NvOdmImagerPowerLevel_On)
            {
                pContext->DelayedFPos = (NvS32) Position;
                Status = NV_TRUE;
                break;
            }

            Status = setPosition(pContext, Position);
            break;
        }
        case NvOdmImagerParameter_FocuserCapabilities:
        {
            NvOdmImagerFocuserCapabilities *pCaps =
                    (NvOdmImagerFocuserCapabilities *)pValue;

            if (pCaps->rangeEndsReversed != INT_MAX)
                pContext->Config.range_ends_reversed = pCaps->rangeEndsReversed;

            if (pCaps->positionWorkingLow != AF_POS_INVALID_VALUE)
                pContext->Config.pos_working_low = pCaps->positionWorkingLow;

            if (pCaps->positionWorkingHigh != AF_POS_INVALID_VALUE)
                pContext->Config.pos_working_high = pCaps->positionActualHigh;

            // The actual low and high are part of the ODM. Blocks-camera may not
            // know the first time, in which case, it will send down invalid.
            // Do not overwrite in that case.
            if(pCaps->positionActualLow != AF_POS_INVALID_VALUE)
                pContext->Config.pos_actual_low = pCaps->positionActualLow;

            if(pCaps->positionActualHigh != AF_POS_INVALID_VALUE)
                pContext->Config.pos_actual_high = pCaps->positionActualHigh;

            if(pCaps->slewRate != INT_MAX)
                pContext->Config.slew_rate = pCaps->slewRate;

            if(pCaps->circleOfConfusion != INT_MAX)
                pContext->Config.circle_of_confusion = pCaps->circleOfConfusion;

            if(pCaps->afConfigSet[0].settle_time != INT_MAX)
                pContext->Config.focuser_set[0].settle_time = pCaps->afConfigSet[0].settle_time;

            FOCUSER_DB_PRINTF("AD5823 %s: pCaps->afConfigSetSize %d",
                        __FUNCTION__, pCaps->afConfigSetSize);
            FOCUSER_DB_PRINTF("AD5823 %s: posActualLow %d high %d positionWorkingLow %d"
                            " positionWorkingHigh %d settle_time %d slew_rate %d", __func__,
                            pContext->Config.pos_actual_low,
                            pContext->Config.pos_actual_high,
                            pCaps->positionWorkingLow,
                            pCaps->positionWorkingHigh,
                            pContext->Config.focuser_set[0].settle_time,
                            pCaps->slewRate);

            pContext->Config.num_focuser_sets = pCaps->afConfigSetSize;

            for (i = 0; i < pCaps->afConfigSetSize; i++)
            {
                pContext->Config.focuser_set[i].posture =
                                        pCaps->afConfigSet[i].posture;
                if(pCaps->afConfigSet[i].macro != AF_POS_INVALID_VALUE)
                    pContext->Config.focuser_set[i].macro =
                                            pCaps->afConfigSet[i].macro;
                if(pCaps->afConfigSet[i].hyper != AF_POS_INVALID_VALUE)
                    pContext->Config.focuser_set[i].hyper =
                                            pCaps->afConfigSet[i].hyper;
                if(pCaps->afConfigSet[i].inf != AF_POS_INVALID_VALUE)
                    pContext->Config.focuser_set[i].inf =
                                            pCaps->afConfigSet[i].inf;
                if(pCaps->afConfigSet[i].hysteresis != INT_MAX)
                    pContext->Config.focuser_set[i].hysteresis =
                                            pCaps->afConfigSet[i].hysteresis;
                if(pCaps->afConfigSet[i].settle_time != INT_MAX)
                    pContext->Config.focuser_set[i].settle_time =
                                            pCaps->afConfigSet[i].settle_time;
                pContext->Config.focuser_set[i].macro_offset =
                                        pCaps->afConfigSet[i].macro_offset;
                pContext->Config.focuser_set[i].inf_offset =
                                        pCaps->afConfigSet[i].inf_offset;
                pContext->Config.focuser_set[i].num_dist_pairs =
                                        pCaps->afConfigSet[i].num_dist_pairs;

                FOCUSER_DB_PRINTF("AD5823 i %d posture %d macro %d hyper %d inf %d "
                                 "hyst %d setttle_time %d macro_offset %d "
                                 "inf_offset %d num_pairs %d",
                    i, pCaps->afConfigSet[i].posture, pCaps->afConfigSet[i].macro,
                    pCaps->afConfigSet[i].hyper, pCaps->afConfigSet[i].inf,
                    pCaps->afConfigSet[i].hysteresis, pCaps->afConfigSet[i].settle_time,
                    pCaps->afConfigSet[i].macro_offset, pCaps->afConfigSet[i].inf_offset,
                    pCaps->afConfigSet[i].num_dist_pairs);

                for (j = 0; j < pCaps->afConfigSet[i].num_dist_pairs; j++)
                {
                    pContext->Config.focuser_set[i].dist_pair[j].fdn =
                                pCaps->afConfigSet[i].dist_pair[j].fdn;
                    pContext->Config.focuser_set[i].dist_pair[j].distance =
                                pCaps->afConfigSet[i].dist_pair[j].distance;

                    FOCUSER_DB_PRINTF("AD5823 i %d j %d fdn %d distance %d", i, j,
                                pCaps->afConfigSet[i].dist_pair[j].fdn,
                                pCaps->afConfigSet[i].dist_pair[j].distance);
                }
            }

            if (ioctl(pContext->focuser_fd, AD5823_IOCTL_SET_CONFIG,
                    &pContext->Config) < 0)
            {
                FOCUSER_DB_PRINTF("DW9718 %s: ioctl to set configrations failed - %s\n",
                                __FILE__, strerror(errno));
                break;
            }

            Status = NV_TRUE;
            break;
        }

        default:
            NvOsDebugPrintf("Unsupported param (%d)\n", Param);
    }

    if (Status != NV_TRUE) {
        NvOsDebugPrintf("%s: %d Failed\n", __FUNCTION__, Param);
    }
    return Status;
}

/**
 * Focuser_GetParameter
 * Get focuser's parameter
 */
static NvBool FocuserAD5823_GetParameter(NvOdmImagerHandle hImager,
                NvOdmImagerParameter Param,
                NvS32 SizeOfValue,
                void *pValue)
{
    NvF32 *pFValue = (NvF32*) pValue;
    NvU32 *pUValue = (NvU32*) pValue;
    FocuserCtxt *pContext = (FocuserCtxt *) hImager->pFocuser->pPrivateContext;

    FOCUSER_DB_PRINTF("Focuser_GetParameter (%d)\n", Param);

    switch(Param) {
        case NvOdmImagerParameter_FocuserLocus:
            /* The last requested position has been settled? */
            Focuser_UpdateSettledPosition(pContext);
            *pUValue = pContext->Position;
            FOCUSER_DB_PRINTF("position = %d\n", *pUValue);
            break;

        case NvOdmImagerParameter_FocalLength:
            *pFValue = pContext->Config.focal_length;
            FOCUSER_DB_PRINTF("focal length = %f\n", *pFValue);
            break;

        case NvOdmImagerParameter_MaxAperture:
            *pFValue = pContext->Config.focal_length / pContext->Config.fnumber;
            FOCUSER_DB_PRINTF("max aperture = %f\n", *pFValue);
            break;

        case NvOdmImagerParameter_FNumber:
            *pFValue = pContext->Config.fnumber;
            FOCUSER_DB_PRINTF("fnumber = %f\n", *pFValue);
            break;

        case NvOdmImagerParameter_FocuserCapabilities:
        {
            NvOdmImagerFocuserCapabilities *pCaps =
            (NvOdmImagerFocuserCapabilities *) pValue;
            NvU32 i;

            if (ioctl(pContext->focuser_fd,
                     AD5823_IOCTL_GET_CONFIG,
                     &pContext->Config) < 0)
            {
                FOCUSER_DB_PRINTF("DW9718 %s: ioctl to Get configrations failed - %s\n",
                            __FILE__, strerror(errno));
                break;
            }

            pCaps->version =  NVODMIMAGER_AF_FOCUSER_CODE_VERSION;
            pCaps->rangeEndsReversed = pContext->Config.range_ends_reversed;

            pCaps->positionActualLow = pContext->Config.pos_actual_low;
            pCaps->positionActualHigh = pContext->Config.pos_actual_high;

            pCaps->positionWorkingLow = pContext->Config.pos_working_low;
            pCaps->positionWorkingHigh = pContext->Config.pos_working_high;
            pCaps->slewRate =  pContext->Config.slew_rate;
            pCaps->circleOfConfusion =  pContext->Config.circle_of_confusion;

            for (i = 0; i < pContext->Config.num_focuser_sets; i++)
            {
                pCaps->afConfigSet[i].posture = pContext->Config.focuser_set[i].posture;
                pCaps->afConfigSet[i].macro = pContext->Config.focuser_set[i].macro;
                pCaps->afConfigSet[i].hyper = pContext->Config.focuser_set[i].hyper;
                pCaps->afConfigSet[i].inf = pContext->Config.focuser_set[i].inf;
                pCaps->afConfigSet[i].hysteresis = pContext->Config.focuser_set[i].hysteresis;
                pCaps->afConfigSet[i].settle_time = pContext->Config.focuser_set[i].settle_time;
                pCaps->afConfigSet[i].macro_offset = pContext->Config.focuser_set[i].macro_offset;
                pCaps->afConfigSet[i].inf_offset = pContext->Config.focuser_set[i].inf_offset;
            }
            FOCUSER_DB_PRINTF("AD5823 %s: Actual low %d high %d, Working low %d "
                             "high %d inf %d macro %d slewrate %d settleTime %d\n",
                            __func__, pCaps->positionActualLow,
                            pCaps->positionActualHigh,
                            pCaps->positionWorkingLow,
                            pCaps->positionWorkingHigh,
                            pCaps->afConfigSet[0].inf,
                            pCaps->afConfigSet[0].macro,
                            pCaps->slewRate,
                            pCaps->afConfigSet[0].settle_time);

        }
            break;
        default:
            NvOsDebugPrintf("Unsupported param (%d)\n", Param);
            return NV_FALSE;
    }

    return NV_TRUE;
}

/**
 * Focuser_GetStaticProperties
 * Get focuser's static properties
 */
static NvBool FocuserAD5823_GetStaticProperties(
        NvOdmImagerHandle hImager,
        NvOdmImagerStaticProperty *pProperties)
{
    FOCUSER_DB_PRINTF("%s\n", __func__);
    if (!hImager || !hImager->pFocuser || !hImager->pFocuser->pPrivateContext ||
            !pProperties)
    {
        NvOsDebugPrintf("%s: Memory not allocated, returning false",
                __FUNCTION__);
        return NV_FALSE;
    }
    FocuserCtxt *pContext = (FocuserCtxt *)hImager->pFocuser->pPrivateContext;
    pProperties->LensAvailableApertures.Values[0] =
        pContext->Config.focal_length / pContext->Config.fnumber;
    pProperties->LensAvailableApertures.Size = 1;
    pProperties->LensAvailableFocalLengths.Values[0] =
        pContext->Config.focal_length;
    pProperties->LensAvailableFocalLengths.Size = 1;
    // minimum focus distance can't be zero if there is a focuser present
    if (pProperties->LensMinimumFocusDistance == 0)
    {
        pProperties->LensMinimumFocusDistance = 0.1f;
    }
    return NV_TRUE;
}
/**
 * FocuserAD5823_GetHal.
 * return the hal functions associated with focuser
 */
NvBool FocuserAD5823_GetHal(NvOdmImagerHandle hImager)
{
    if (!hImager || !hImager->pFocuser)
        return NV_FALSE;

    hImager->pFocuser->pfnOpen = FocuserAD5823_Open;
    hImager->pFocuser->pfnClose = FocuserAD5823_Close;
    hImager->pFocuser->pfnGetCapabilities = FocuserAD5823_GetCapabilities;
    hImager->pFocuser->pfnSetPowerLevel = FocuserAD5823_SetPowerLevel;
    hImager->pFocuser->pfnSetParameter = FocuserAD5823_SetParameter;
    hImager->pFocuser->pfnGetParameter = FocuserAD5823_GetParameter;
    hImager->pFocuser->pfnStaticQuery = FocuserAD5823_GetStaticProperties;

    return NV_TRUE;
}
#endif
