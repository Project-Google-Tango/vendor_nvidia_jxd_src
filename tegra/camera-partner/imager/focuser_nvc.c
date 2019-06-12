/*
 * Copyright (c) 2011-2013, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#if (BUILD_FOR_AOS == 0)
#ifndef NV_ENABLE_DEBUG_PRINTS
#define NV_ENABLE_DEBUG_PRINTS (0)
#endif

#include <linux/ioctl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <asm/types.h>
#include <nvc.h>
#include <nvc_focus.h>

#include "focuser_nvc.h"
#include "imager_hal.h"
#include "imager_util.h"
#include "focuser_common.h"


typedef struct FocuserNvcContextRec
{
    int focus_fd; /* file handle to the kernel nvc driver */
    struct nv_focuser_config Config;
    // focuser capabilities
    NvOdmImagerFocuserCapabilities Caps;
    NvF32 FocalLength;
    NvF32 MaxAperture;
    NvF32 FNumber;
} FocuserNvcContext;

static NvBool SetFocuserCapabilities(struct nv_focuser_config *pCfg, NvOdmImagerFocuserCapabilities *pCaps);
static void GetFocuserCapabilities(struct nv_focuser_config *pCfg, NvOdmImagerFocuserCapabilities *pCaps);

static int
FocuserNvc_IOGetCap(
    int fd,
    struct nv_focuser_config *pcfg,
    NvOdmImagerFocuserCapabilities *pcaps)
{
    struct nvc_param params;
    int err;

    if (fd < 0 || pcfg == NULL || pcaps == NULL)
    {
        return -EINVAL;
    }

    // get the focuser capabilities
    params.param = NVC_PARAM_CAPS;
    params.p_value = pcfg;
    params.sizeofvalue = sizeof(*pcfg);
    err = ioctl(fd, NVC_IOCTL_PARAM_RD, &params);
    if (err < 0)
    {
        NvOsDebugPrintf("%s-%d: ioctl to get caps failed: %s\n",
                        __func__, __LINE__, strerror(errno));
        return err;
    }
    GetFocuserCapabilities(pcfg, pcaps);

    return 0;
}

static NvBool
FocuserNvc_Open(
    NvOdmImagerHandle hImager)
{
    FocuserNvcContext *pCtxt;
    unsigned Instance;
    char DevName[NVODMIMAGER_IDENTIFIER_MAX];
    struct nvc_param params;
    int err;

    if (!hImager || !hImager->pFocuser)
    {
        NvOsDebugPrintf("%s: No hImager->pFocuser\n",
                        __func__);
        return NV_FALSE;
    }

    pCtxt = (FocuserNvcContext *)NvOdmOsAlloc(sizeof(*pCtxt));
    if (!pCtxt)
    {
        NvOsDebugPrintf("%s: Memory allocation failed\n", __func__);
        return NV_FALSE;
    }
    memset(pCtxt, 0, sizeof(*pCtxt));

    Instance = (unsigned)(hImager->pFocuser->GUID & 0xF);
    if (Instance)
        sprintf(DevName, "/dev/focuser.%u", Instance);
    else
        sprintf(DevName, "/dev/focuser");

    hImager->pFocuser->pPrivateContext = NULL;
#ifdef O_CLOEXEC
    pCtxt->focus_fd = open(DevName, O_RDWR|O_CLOEXEC);
#else
    pCtxt->focus_fd = open(DevName, O_RDWR);
#endif
    if (pCtxt->focus_fd < 0)
    {
        NvOdmOsFree(pCtxt);
        NvOsDebugPrintf("%s: cannot open focuser driver: %s Err: %s\n",
                        __func__, DevName, strerror(errno));
        return NV_FALSE;
    }
    // NvOsDebugPrintf("%s: %s driver focus_fd opened as: %d\n",
                   // __func__, DevName, pCtxt->focus_fd);

    // get the focuser capabilities
    err = FocuserNvc_IOGetCap(pCtxt->focus_fd, &pCtxt->Config, &pCtxt->Caps);
    if (err < 0)
    {
        NvOdmOsFree(pCtxt);
        return NV_FALSE;
    }

    // get the focuser focal length
    params.param = NVC_PARAM_FOCAL_LEN;
    params.p_value = &pCtxt->FocalLength;
    params.sizeofvalue = sizeof(pCtxt->FocalLength);
    err = ioctl(pCtxt->focus_fd, NVC_IOCTL_PARAM_RD, &params);
    if (err < 0)
    {
        NvOsDebugPrintf("%s-%d: ioctl to get focal len failed: %s\n",
                        __func__, __LINE__, strerror(errno));
        NvOdmOsFree(pCtxt);
        return NV_FALSE;
    }

    // get the focuser focal length
    params.param = NVC_PARAM_MAX_APERTURE;
    params.p_value = &pCtxt->MaxAperture;
    params.sizeofvalue = sizeof(pCtxt->MaxAperture);
    err = ioctl(pCtxt->focus_fd, NVC_IOCTL_PARAM_RD, &params);
    if (err < 0)
    {
        NvOsDebugPrintf("%s-%d: ioctl to get max aperture failed: %s\n",
                        __func__, __LINE__, strerror(errno));
        NvOdmOsFree(pCtxt);
        return NV_FALSE;
    }

    // get the focuser fnumber
    params.param = NVC_PARAM_FNUMBER;
    params.p_value = &pCtxt->FNumber;
    params.sizeofvalue = sizeof(pCtxt->FNumber);
    err = ioctl(pCtxt->focus_fd, NVC_IOCTL_PARAM_RD, &params);
    if (err < 0)
    {
        NvOsDebugPrintf("%s-%d: ioctl to get fnumber failed: %s\n",
                        __func__, __LINE__, strerror(errno));
        NvOdmOsFree(pCtxt);
        return NV_FALSE;
    }

    hImager->pFocuser->pPrivateContext = pCtxt;
    return NV_TRUE;
}

static void
FocuserNvc_Close(
    NvOdmImagerHandle hImager)
{
    FocuserNvcContext *pCtxt;

    if (!hImager || !hImager->pFocuser || !hImager->pFocuser->pPrivateContext)
    {
        NvOsDebugPrintf("%s: No hImager->pFocuser->pPrivateContext\n",
                        __func__);
        return;
    }

    pCtxt = (FocuserNvcContext *)hImager->pFocuser->pPrivateContext;
    close(pCtxt->focus_fd);
    NvOdmOsFree(pCtxt);
    hImager->pFocuser->pPrivateContext = NULL;
}

static NvBool
FocuserNvc_SetPowerLevel(
    NvOdmImagerHandle hImager,
    NvOdmImagerPowerLevel PowerLevel)
{
    FocuserNvcContext *pCtxt;
    int err;

    if (!hImager || !hImager->pFocuser || !hImager->pFocuser->pPrivateContext)
    {
        NvOsDebugPrintf("%s: No hImager->pFocuser->pPrivateContext\n",
                        __func__);
        return NV_FALSE;
    }

    pCtxt = (FocuserNvcContext *)hImager->pFocuser->pPrivateContext;
    err = ioctl(pCtxt->focus_fd, NVC_IOCTL_PWR_WR, PowerLevel);
    if (err < 0)
    {
        NvOsDebugPrintf("%s: ioctl to set power level (%d) failed: %s\n",
                        __func__, PowerLevel, strerror(errno));
        return NV_FALSE;
    }

    return NV_TRUE;
}

static void
FocuserNvc_GetCapabilities(
    NvOdmImagerHandle hImager,
    NvOdmImagerCapabilities *pCapabilities)
{
}

static NvBool
FocuserNvc_SetParameter(
    NvOdmImagerHandle hImager,
    NvOdmImagerParameter Param,
    NvS32 SizeOfValue,
    const void *pValue)
{
    FocuserNvcContext *pCtxt;
    struct nvc_param params;
    int err;

    if (!hImager || !hImager->pFocuser || !hImager->pFocuser->pPrivateContext)
    {
        NvOsDebugPrintf("%s: No hImager->pFocuser->pPrivateContext\n",
                        __func__);
        return NV_FALSE;
    }

    pCtxt = (FocuserNvcContext *)hImager->pFocuser->pPrivateContext;
    params.param = Param;
    params.sizeofvalue = SizeOfValue;
    params.p_value = (void *)pValue;
    switch(Param) {
        case NvOdmImagerParameter_FocuserLocus:
            params.param = NVC_PARAM_LOCUS;
            break;

        case NvOdmImagerParameter_FocuserStereo:
        case NvOdmImagerParameter_StereoCameraMode:
            params.param = NVC_PARAM_STEREO;
            break;

        case NvOdmImagerParameter_Reset:
            params.param = NVC_PARAM_RESET;
            break;

        case NvOdmImagerParameter_SelfTest:
            params.param = NVC_PARAM_SELF_TEST;
            break;

        case NvOdmImagerParameter_FocuserCapabilities:
            if(SetFocuserCapabilities(&pCtxt->Config, (NvOdmImagerFocuserCapabilities *)pValue) == NV_FALSE)
            {
                NvOsDebugPrintf("%s: SetFocuserCapabilities failed\n",
                                __func__);
                return NV_FALSE;
            }
            params.param = NVC_PARAM_CAPS;
            params.sizeofvalue = sizeof(pCtxt->Config);
            params.p_value = (void *) &pCtxt->Config;
            break;

        default:
            NvOsDebugPrintf("%s: ioctl default case\n", __func__);
            break;
    }

    err = ioctl(pCtxt->focus_fd, NVC_IOCTL_PARAM_WR, &params);
    if (err < 0)
    {
        NvOsDebugPrintf("%s: ioctl to set parameter failed: %s\n",
                        __func__, strerror(errno));
        return NV_FALSE;
    }

    if (Param == NvOdmImagerParameter_FocuserCapabilities)
    {
        err = FocuserNvc_IOGetCap(pCtxt->focus_fd, &pCtxt->Config, &pCtxt->Caps);
        if (err < 0)
        {
            return NV_FALSE;
        }
    }

    return NV_TRUE;
}


static void GetFocuserCapabilities(struct nv_focuser_config *pCfg,
                                NvOdmImagerFocuserCapabilities *pCaps)
{
    NvU32 i, j;

    memset(pCaps, 0, sizeof(*pCaps));
    pCaps->version = NVODMIMAGER_AF_FOCUSER_CODE_VERSION;

    pCaps->rangeEndsReversed = pCfg->range_ends_reversed;
    pCaps->positionWorkingLow = pCfg->pos_working_low;
    pCaps->positionWorkingHigh = pCfg->pos_working_high;

    pCaps->positionActualLow = pCfg->pos_actual_low;
    pCaps->positionActualHigh = pCfg->pos_actual_high;

    pCaps->slewRate = pCfg->slew_rate;
    pCaps->circleOfConfusion = pCfg->circle_of_confusion;

    NV_DEBUG_PRINTF(("%s: pCaps->afConfigSetSize %d",
                __FUNCTION__, pCaps->afConfigSetSize));
    NV_DEBUG_PRINTF(("%s: posActualLow %d high %d positionWorkingLow %d"
                    " positionWorkingHigh %d", __FUNCTION__,
                    pCaps->positionActualLow,
                    pCaps->positionActualHigh,
                    pCaps->positionWorkingLow,
                    pCaps->positionWorkingHigh));

    for (i = 0; i < pCfg->num_focuser_sets; i++)
    {
        pCaps->afConfigSet[i].posture = pCfg->focuser_set[i].posture;
        pCaps->afConfigSet[i].macro = pCfg->focuser_set[i].macro;
        pCaps->afConfigSet[i].hyper = pCfg->focuser_set[i].hyper;
        pCaps->afConfigSet[i].inf = pCfg->focuser_set[i].inf;
        pCaps->afConfigSet[i].hysteresis = pCfg->focuser_set[i].hysteresis;
        pCaps->afConfigSet[i].settle_time = pCfg->focuser_set[i].settle_time;
        pCaps->afConfigSet[i].macro_offset = pCfg->focuser_set[i].macro_offset;
        pCaps->afConfigSet[i].inf_offset = pCfg->focuser_set[i].inf_offset;
        pCaps->afConfigSet[i].num_dist_pairs = pCfg->focuser_set[i].num_dist_pairs;

        NV_DEBUG_PRINTF(("i %d posture %d macro %d hyper %d inf %d "
                         "hyst %d setttle_time %d macro_offset %d "
                         "inf_offset %d num_pairs %d",
            i, pCaps->afConfigSet[i].posture, pCaps->afConfigSet[i].macro,
            pCaps->afConfigSet[i].hyper, pCaps->afConfigSet[i].inf,
            pCaps->afConfigSet[i].hysteresis, pCaps->afConfigSet[i].settle_time,
            pCaps->afConfigSet[i].macro_offset, pCaps->afConfigSet[i].inf_offset,
            pCaps->afConfigSet[i].num_dist_pairs));

        for (j = 0; j < pCaps->afConfigSet[i].num_dist_pairs; j++)
        {
            pCaps->afConfigSet[i].dist_pair[j].fdn = pCfg->focuser_set[i].dist_pair[j].fdn;
            pCaps->afConfigSet[i].dist_pair[j].distance = pCfg->focuser_set[i].dist_pair[j].distance;

            NV_DEBUG_PRINTF(("i %d j %d fdn %d distance %d", i, j,
                        pCaps->afConfigSet[i].dist_pair[j].fdn,
                        pCaps->afConfigSet[i].dist_pair[j].distance));
        }
    }
}

static NvBool SetFocuserCapabilities(
    struct nv_focuser_config *pCfg, NvOdmImagerFocuserCapabilities *pCaps)
{
    NvU32 i, j;

    if(pCaps->version != NVODMIMAGER_AF_FOCUSER_CODE_VERSION)
    {
        //
        // Version mismatch. Serious error. Log error and bail out
        //
        NvOsDebugPrintf("%s: Error: Capabilities structure version mismatch. "
               "Expected %d Passed %d\n", NVODMIMAGER_AF_FOCUSER_CODE_VERSION,
               pCaps->version);
        return NV_FALSE;
    }

    //
    // focuser_nvc.c uses the new focuser framework, and its a pass through layer
    // to kernel on one side and blocks-camera on the top. It just keeps a copy
    // of the buffer here. So, we cleanup the buffer and blindly copy the contents.
    // There is no need to check each variable for INT_MAX in most cases.
    memset(pCfg, 0, sizeof(*pCfg));

    pCfg->range_ends_reversed = pCaps->rangeEndsReversed;

    pCfg->pos_working_low = pCaps->positionWorkingLow;
    pCfg->pos_working_high = pCaps->positionWorkingHigh;
    pCfg->pos_actual_low = pCaps->positionActualLow;
    pCfg->pos_actual_high = pCaps->positionActualHigh;

    pCfg->slew_rate = pCaps->slewRate;
    pCfg->circle_of_confusion = pCaps->circleOfConfusion;

    NV_DEBUG_PRINTF(("%s: pCaps->afConfigSetSize %d",
                __FUNCTION__, pCaps->afConfigSetSize));
    NV_DEBUG_PRINTF(("%s: posActualLow %d high %d positionWorkingLow %d"
                    " positionWorkingHigh %d", __FUNCTION__,
                    pCfg->pos_actual_low,
                    pCfg->pos_actual_high,
                    pCfg->pos_working_low,
                    pCfg->pos_working_high));

    for (i = 0; i < pCaps->afConfigSetSize; i++)
    {
        pCfg->focuser_set[i].posture = pCaps->afConfigSet[i].posture;
        pCfg->focuser_set[i].macro = pCaps->afConfigSet[i].macro;
        pCfg->focuser_set[i].hyper = pCaps->afConfigSet[i].hyper;
        pCfg->focuser_set[i].inf = pCaps->afConfigSet[i].inf;
        pCfg->focuser_set[i].hysteresis = pCaps->afConfigSet[i].hysteresis;
        pCfg->focuser_set[i].settle_time = pCaps->afConfigSet[i].settle_time;
        pCfg->focuser_set[i].macro_offset = pCaps->afConfigSet[i].macro_offset;
        pCfg->focuser_set[i].inf_offset = pCaps->afConfigSet[i].inf_offset;
        pCfg->focuser_set[i].num_dist_pairs = pCaps->afConfigSet[i].num_dist_pairs;

        NV_DEBUG_PRINTF(("i %d posture %d macro %d hyper %d inf %d "
                         "hyst %d setttle_time %d macro_offset %d "
                         "inf_offset %d num_pairs %d",
            i, pCaps->afConfigSet[i].posture, pCaps->afConfigSet[i].macro,
            pCaps->afConfigSet[i].hyper, pCaps->afConfigSet[i].inf,
            pCaps->afConfigSet[i].hysteresis, pCaps->afConfigSet[i].settle_time,
            pCaps->afConfigSet[i].macro_offset, pCaps->afConfigSet[i].inf_offset,
            pCaps->afConfigSet[i].num_dist_pairs));

        for (j = 0; j < pCaps->afConfigSet[i].num_dist_pairs; j++)
        {
            pCfg->focuser_set[i].dist_pair[j].fdn = pCaps->afConfigSet[i].dist_pair[j].fdn;
            pCfg->focuser_set[i].dist_pair[j].distance = pCaps->afConfigSet[i].dist_pair[j].distance;

            NV_DEBUG_PRINTF(("i %d j %d fdn %d distance %d", i, j,
                        pCaps->afConfigSet[i].dist_pair[j].fdn,
                        pCaps->afConfigSet[i].dist_pair[j].distance));
        }
    }

    return NV_TRUE;

}

static NvBool
FocuserNvc_GetParameter(
    NvOdmImagerHandle hImager,
    NvOdmImagerParameter Param,
    NvS32 SizeOfValue,
    void *pValue)
{
    FocuserNvcContext *pCtxt;
    struct nvc_param params;
    int err;

    if (!hImager || !hImager->pFocuser || !hImager->pFocuser->pPrivateContext)
    {
        NvOsDebugPrintf("%s: No hImager->pFocuser->pPrivateContext\n",
                        __func__);
        return NV_FALSE;
    }

    pCtxt = (FocuserNvcContext*)hImager->pFocuser->pPrivateContext;
    switch(Param) {
        case NvOdmImagerParameter_FocuserLocus:
            params.param = NVC_PARAM_LOCUS;
            params.sizeofvalue = SizeOfValue;
            params.p_value = pValue;
            err = ioctl(pCtxt->focus_fd, NVC_IOCTL_PARAM_RD, &params);
            if (err < 0)
            {
                NvOsDebugPrintf("%s: ioctl to get parameter failed: %s\n",
                        __func__, strerror(errno));
                return NV_FALSE;
            }
            break;

        case NvOdmImagerParameter_FocalLength:
            memcpy(pValue, &pCtxt->FocalLength, SizeOfValue);
            break;

        case NvOdmImagerParameter_MaxAperture:
            memcpy(pValue, &pCtxt->MaxAperture, SizeOfValue);
            break;

        case NvOdmImagerParameter_FNumber:
            memcpy(pValue, &pCtxt->FNumber, SizeOfValue);
            break;

        case NvOdmImagerParameter_FocuserCapabilities:
            memcpy(pValue, &pCtxt->Caps, SizeOfValue);
            break;

        default:
            break;
    }

    return NV_TRUE;
}

static NvBool FocuserNvc_GetStaticProperties(
    NvOdmImagerHandle hImager,
    NvOdmImagerStaticProperty *pProperties)
{
    NvBool Status;
    NvBool bNewInst = NV_FALSE;
    FocuserNvcContext *pCtxt;

    NV_DEBUG_PRINTF(("%s\n", __func__));
    if (!hImager || !hImager->pFocuser)
        return NV_FALSE;

    // skip in the case camera-core already opened the device
    if (!hImager->pFocuser->pPrivateContext)
    {
        Status = FocuserNvc_Open(hImager);
        if (Status == NV_FALSE)
        {
            return NV_FALSE;
        }
        bNewInst = NV_TRUE;
    }

    pCtxt = (FocuserNvcContext*)hImager->pFocuser->pPrivateContext;
    pProperties->LensAvailableApertures.Values[0] = pCtxt->MaxAperture;
    pProperties->LensAvailableApertures.Size = 1;
    pProperties->LensAvailableFocalLengths.Values[0] = pCtxt->FocalLength;
    pProperties->LensAvailableFocalLengths.Size = 1;
    // minimum focus distance can't be zero if there is a focuser present
    if (pProperties->LensMinimumFocusDistance == 0)
    {
        pProperties->LensMinimumFocusDistance = 0.1f;
    }

    /* these focuser properties are not supported currently:
       LensAvailableFilterDensities
       LensAvailableOpticalStabilization, not available
       LensGeometricCorrectionMap, not available
       LensGeometricCorrectionMapSize, not available
       LensHyperfocalDistance, not available
       LensOpticalAxisAngle, not available  90 degree?
       LensPosition, not available
    */

    if (bNewInst)
    {
        FocuserNvc_Close(hImager);
    }

    return NV_TRUE;
}

NvBool
FocuserNvc_GetHal(
    NvOdmImagerHandle hImager)
{
    if (!hImager || !hImager->pFocuser)
    {
        NvOsDebugPrintf("%s: No hImager->pFocuser\n", __func__);
        return NV_FALSE;
    }

    hImager->pFocuser->pfnOpen = FocuserNvc_Open;
    hImager->pFocuser->pfnClose = FocuserNvc_Close;
    hImager->pFocuser->pfnGetCapabilities = FocuserNvc_GetCapabilities;
    hImager->pFocuser->pfnSetPowerLevel = FocuserNvc_SetPowerLevel;
    hImager->pFocuser->pfnSetParameter = FocuserNvc_SetParameter;
    hImager->pFocuser->pfnGetParameter = FocuserNvc_GetParameter;
    hImager->pFocuser->pfnStaticQuery = FocuserNvc_GetStaticProperties;

    return NV_TRUE;
}

#endif //(BUILD_FOR_AOS == 0)

